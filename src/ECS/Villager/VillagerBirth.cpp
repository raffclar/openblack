/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerBirth.h"

#include <algorithm>
#include <string>
#include <utility>

#include <fmt/format.h>

#include "3D/MapCoords.h"
#include "Audio/Audio.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Life.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/VillagerChildFactoryInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerAge.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerTrace.h"
#include "ECS/Weather/Calendar.h"
#include "InfoConstants.h"
#include "Locator.h"

// The pregnancy and the births (VillagerBirth.h)

namespace openblack::ecs::villager
{
using namespace components;

namespace
{
/// Turns per year (36000.0f) / 365.25; 98.5626f in single precision. (approximate) a constant here: the turns per year
/// are also written by SET_TURNS_PER_YEAR, which openblack's land script does not port yet
constexpr float k_TurnsPerDay = weather::calendar::k_TurnsPerYear / weather::calendar::k_NumDaysInYear;
/// BirthCounter's factor and offset
constexpr float k_Quarter = 0.25f;
constexpr float k_One = 1.0f;
/// ChildBorn's rolls: GameRand(100), GameRand(6) + 1 (villager numbers 1 FORESTER .. 6 TRADER)
constexpr uint32_t k_BirthRoll = 100;
constexpr uint32_t k_BirthNumbers = 6;
/// The newborn's age
constexpr uint32_t k_ChildAge = 1;
/// The birth sound: a random sample of ten from the first, not tracked, mode 2, no loops, flag 0x10 off, 3D, bank 1
/// (IN_GAME), no delay
constexpr int k_BirthSampleFirst = 0x14;
constexpr int k_BirthSampleCount = 0xA;
constexpr int k_BirthSampleMode = 2;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

LivingAction* ActionOf(entt::entity villager)
{
	return Entities().TryGet<LivingAction>(villager);
}

/// The villager's abode (entt::null without one, or when it is gone)
entt::entity AbodeOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	// (guard) the original only reads the link: stands for the unlinking when the abode is deleted
	return v != nullptr && v->abode != entt::null && ecs::IsAvailable(v->abode) ? v->abode : entt::null;
}

/// The VillagerInfo row of an info.dat entry (the original passes the info itself)
VillagerInfo RowOf(const GVillagerInfo& info)
{
	const auto& table = Locator::infoConstants::value().villager;
	return static_cast<VillagerInfo>(&info - table.data());
}

entt::entity CreateChild(const glm::vec3& position, VillagerInfo info, uint32_t age)
{
	// The villager child factory (VillagerArchetype in the game) makes the villager with all its random draws, its
	// first state and its map insertion. It joins no town or abode (ChildBorn does it below)
	return Locator::villagerChildFactory::value().CreateChild(position, info, age);
}
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

uint32_t BirthCounterRange()
{
	// Truncated
	return static_cast<uint32_t>(map_coords::FtoL(k_TurnsPerDay));
}

uint16_t BirthCounter(uint32_t r)
{
	// r (unsigned) + turns per day x 0.25, + 1, truncated to a u16
	const float quarter = k_TurnsPerDay * k_Quarter;
	const float sum = static_cast<float>(r) + quarter;
	return static_cast<uint16_t>(map_coords::FtoL(sum + k_One));
}

// ---- the pregnancy -----------------------------------------------------------------------------------------------

bool WillHousewifeGetPregnant(entt::entity villager)
{
	// The abode first; pregnant -> 0; no town -> 0; no abode -> 0; !IsSexuallyActive -> 0
	const auto abode = AbodeOf(villager);
	if (IsPregnant(villager))
	{
		return false;
	}
	const auto town = GetTown(villager);
	if (town == entt::null || abode == entt::null || !IsSexuallyActive(villager))
	{
		return false;
	}
	// s = the town's GetDesireSignificanceToVillager(8 FOR_CHILDREN)
	const float s = town_desire::GetDesireSignificanceToVillager(town, TownDesireInfo::ForChildren);
	// n = the abode's child count (a byte) + the inhabitants that are pregnant
	uint32_t n = 0;
	if (const auto* a = Entities().TryGet<const Abode>(abode))
	{
		n = a->childCount;
	}
	n += static_cast<uint32_t>(
	    std::ranges::count_if(abode_villagers::VillagersOf(abode), [](const auto other) { return IsPregnant(other); }));
	// s <= 0 -> 0; MaxChildrenInAbode <= n -> 0
	const bool yes = s > 0.0f && abode_villagers::MaxChildren(abode) > n;
	villager::TraceFormatted(villager, "birth: WillHousewifeGetPregnant s {:.4f} children {} max {} -> {}", s, n,
	                         abode_villagers::MaxChildren(abode), yes ? 1 : 0);
	return yes;
}

uint32_t HousewifeGetsPregnant(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// pregnancy = TimePregnantFor (a word)
	v->pregnancy = static_cast<int16_t>(static_cast<uint16_t>(InfoOf(villager).timePregnantFor));
	villager::TraceFormatted(villager, "birth: pregnant for {} turns", static_cast<uint16_t>(v->pregnancy));
	// Not at home -> GoHome (its result)
	if (!IsAtHome(villager))
	{
		return GoHome(villager);
	}
	// (approximate) the original returns a pointer here, never 0; its callers ignore it
	return 1;
}

// ---- the birth ---------------------------------------------------------------------------------------------------

uint32_t HousewifeStartsGivingBirth(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// pregnancy = 0, first
	v->pregnancy = 0;
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return 0; // (guard) every villager has its LivingAction
	}
	// counter = BirthCounter(GameRand(truncated turns per day))
	const uint32_t r = GameRand(BirthCounterRange());
	action->turnsUntilStateChange = BirthCounter(r);
	villager::TraceFormatted(villager, "birth: starts giving birth, counter {}", action->turnsUntilStateChange);
	// SetTopState(111 HOUSEWIFE_GIVING_BIRTH)
	SetTopState(villager, VillagerStates::HousewifeGivingBirth);
	// HousewifeGivingBirth (its result)
	return HousewifeGivingBirth(villager);
}

uint32_t HousewifeStartsGivingBirthState(LivingAction& action)
{
	return HousewifeStartsGivingBirth(Entities().ToEntity(action));
}

uint32_t HousewifeGivingBirth(entt::entity villager)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return 1;
	}
	// --counter (a word); not 0 -> 1
	--action->turnsUntilStateChange;
	if (action->turnsUntilStateChange != 0)
	{
		return 1;
	}
	const auto child = ChildBorn(villager);
	// A child -> the birth sound at its position
	if (child != entt::null)
	{
		audio::tags::CreateAtMapCoords(object::MapCoordsOf(child),
		                               audio::tags::RandomSample(k_BirthSampleFirst, k_BirthSampleCount), false,
		                               k_BirthSampleMode, 0, false, true, audio::SfxBank::InGame, 0);
	}
	// SetTopState(112 HOUSEWIFE_GIVEN_BIRTH); 1
	SetTopState(villager, VillagerStates::HousewifeGivenBirth);
	return 1;
}

uint32_t HousewifeGivingBirthState(LivingAction& action)
{
	return HousewifeGivingBirth(Entities().ToEntity(action));
}

uint32_t HousewifeGivenBirth(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// pregnancy (a word) = 0
	if (auto* v = VillagerOf(villager))
	{
		v->pregnancy = 0;
	}
	return GoHome(villager);
}

entt::entity ChildBorn(entt::entity mother)
{
	const auto* m = VillagerOf(mother);
	const auto* transform = Entities().TryGet<const Transform>(mother);
	if (m == nullptr || transform == nullptr)
	{
		return entt::null;
	}
	const auto& motherInfo = InfoOf(mother);
	// r = GameRand(100), compared with BoyGirlChance (unsigned): chance < r -> the mother's info
	const uint32_t r = GameRand(k_BirthRoll);
	const GVillagerInfo* info = &motherInfo;
	if (r <= motherInfo.boyGirlChance)
	{
		// FindVillagerInfo(the mother's tribe type, GameRand(6) + 1); null -> the mother's
		const auto number = static_cast<VillagerNumber>(GameRand(k_BirthNumbers) + 1);
		if (const auto* found = FindVillagerInfo(motherInfo.tribeType, number); found != nullptr)
		{
			info = found;
		}
	}
	// A child at the mother's position, of that info, age 1; null -> null.
	// (approximate, P12) the position from the mother's Transform
	const auto child = CreateChild(transform->position, RowOf(*info), k_ChildAge);
	if (child == entt::null)
	{
		return entt::null;
	}
	// The constructor's SetSkeleton (VillagerCore.cpp Construct: (pending) not called there).
	// (approximate) called here for a skeleton mother only, so its SetScaleForAge draw happens only then
	if (IsSkeleton(mother))
	{
		SetSkeleton(child, true);
	}
	// (pending, multiplayer) a multiplayer game with a player updates a per-player record. openblack has no
	// multiplayer
	// The mother's abode -> AddVillagerToAbode; else her town -> AddVillagerToTown; else the head of the vagrants
	// unless it is in the list already
	if (const auto abode = AbodeOf(mother); abode != entt::null)
	{
		abode_villagers::AddVillagerToAbode(abode, child);
	}
	else if (const auto town = GetTown(mother); town != entt::null)
	{
		town_villagers::AddVillagerToTown(town, child);
	}
	else
	{
		town_villagers::AddToVagrants(child);
	}
	// The child's mother
	if (auto* c = VillagerOf(child))
	{
		c->mother = mother;
	}
	// The player's "total births" stat ++. TODO(Intro): game_stats::ChildBorn(size_t player) (src/Game/GameStats.h,
	// not in HEAD)
	if (const auto player = GetPlayerOf(mother); player.has_value())
	{
		villager::TraceFormatted(mother, "birth: GameStats +0x48 of player {} (TODO(Intro))", static_cast<int>(*player));
	}
	// A poisoned mother -> a poisoned child
	if (life::IsPoisoned(mother))
	{
		life::SetPoisoned(child, true);
	}
	villager::TraceFormatted(mother, "birth: child {} (r {}, row {})", static_cast<uint32_t>(child), r,
	                         static_cast<int>(RowOf(*info)));
	ChildDecideWhatToDo(child);
	return child;
}
} // namespace openblack::ecs::villager
