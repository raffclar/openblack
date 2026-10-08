/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerSpeed.h"

#include <algorithm>
#include <array>

#include <entt/entity/entity.hpp>
#include <fmt/format.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerAge.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerResources.h"
#include "Game.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "LandBalance.h"
#include "Locator.h"

namespace openblack::ecs
{
using namespace components;

namespace
{
/// VillagerDisciple 9 TRADER
constexpr uint8_t k_DiscipleTrader = 9;

/// The game turn
uint32_t CurrentGameTurn()
{
	return game_clock::Turn();
}

uint32_t Raw(SpeedState state)
{
	return static_cast<uint32_t>(state);
}

uint32_t SpeedGroupEntry(const SpeedGroup& group, uint32_t index)
{
	const std::array<SpeedState, 6> entries = {group.speedDefault, group.speedFleeing, group.speed2,
	                                           group.speed3,       group.speed4,       group.speed5};
	return Raw(entries.at(std::min<uint32_t>(index, 5)));
}
} // namespace

const GVillagerInfo* VillagerInfoOf(entt::entity entity)
{
	const auto* villager = Locator::entitiesRegistry::value().TryGet<const Villager>(entity);
	if (villager == nullptr)
	{
		return nullptr;
	}
	for (const auto& info : Locator::infoConstants::value().villager)
	{
		if (info.tribeType == villager->tribe && info.villagerNumber == villager->number)
		{
			return &info;
		}
	}
	return nullptr;
}

void SetVillagerStateSpeed(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* villager = registry.TryGet<const Villager>(entity);
	const auto* action = registry.TryGet<const LivingAction>(entity);
	auto* wallHug = registry.TryGet<WallHug>(entity);
	const auto* info = VillagerInfoOf(entity);
	if (villager == nullptr || action == nullptr || wallHug == nullptr || info == nullptr)
	{
		return;
	}
	const auto& states = Locator::infoConstants::value().villagerStateTable;
	// GetFinalState: the current state if it is a final one, else the destination
	const auto top = action->states[static_cast<size_t>(LivingAction::Index::Top)];
	// a villager controlled by a script keeps its speed
	if (script_held::IsControlledByScript(entity))
	{
		return;
	}
	// so does a dancing one (one in a dance group). (approximate) openblack has no dance group link yet;
	// WorshipVillager::dancing, set where the original puts it in a dance group and cleared when it leaves the dance,
	// stands for it, and TOP == IN_DANCE for the other dances, as openblack had it
	const auto* worship = registry.TryGet<const WorshipVillager>(entity);
	if (top == static_cast<uint8_t>(VillagerStates::InDance) || (worship != nullptr && worship->dancing))
	{
		return;
	}
	const auto destination = action->states[static_cast<size_t>(LivingAction::Index::Final)];
	auto final = top < states.size() && states[top].isFinalState != 0 ? top : destination;
	if (final >= states.size())
	{
		final = top;
	}
	if (final >= states.size())
	{
		return;
	}
	// m: the land balance speed scale (land_balance value 4: 1.5 in Land2, 1.25 in Land3) * the player's wonder bonus
	// (1) * the town's belief term (openblack has no belief in the player yet: 1)
	const float m = land_balance::Get(4);
	const float life = villager->life;
	const auto& group = info->speedGroup;
	float speed = 0.0f;
	// the emergency and normal branches go through the food speed-up; the two wounded ones skip it
	bool foodSpeedUpApplies = false;
	if (life <= info->lifeWhenCrawlsWounded)
	{
		// GameFloatRand(0.2) + 0.4
		speed = (game_random::GameFloatRand(0.2f) + 0.4f) * static_cast<float>(Raw(group.speed4)) * m;
	}
	else if (life <= info->lifeWhenWalksWounded)
	{
		// GameFloatRand(0.25) + 0.5
		speed = (game_random::GameFloatRand(0.25f) + 0.5f) * static_cast<float>(Raw(group.speedDefault)) * m;
	}
	else if (const auto* town = villager->town != entt::null ? registry.TryGet<const Town>(villager->town) : nullptr;
	         town != nullptr && town_queries::IsInStateOfEmergency(*town))
	{
		// in a town in a state of emergency: (GameFloatRand(0.5) + 0.75) x the fleeing speed x m
		speed = (game_random::GameFloatRand(0.5f) + 0.75f) * static_cast<float>(Raw(group.speedFleeing)) * m;
		foodSpeedUpApplies = true;
	}
	else
	{
		// T = 1 without a town; with one, base + clamp(TownNeedsSum / divisor, 0, 0.5) (float precision: the FPU runs
		// at 24 bits)
		const float townNeeds =
		    villager->town != entt::null ? villager::TownNeedsFactor(town_desire::TownNeedsSum(villager->town), *info) : 1.0f;
		// the loads of wood and food, with the trader's capacities for disciple 9
		const auto load = villager::LoadFactors(villager->resourceHeld.at(1), villager->resourceHeld.at(0), *info,
		                                        villager->discipleType == k_DiscipleTrader);
		// spd x foodF x woodF x T x m, each multiplication rounded to float
		speed = static_cast<float>(static_cast<int32_t>(SpeedGroupEntry(group, states[final].speedIndex))) * load.food *
		        load.wood * townNeeds * m;
		foodSpeedUpApplies = true;
		if (villager::TraceOn(entity))
		{
			villager::Trace(entity, fmt::format("speed: spd {} foodF {:.9f} woodF {:.9f} T {:.9f} -> {:.6f}",
			                                    SpeedGroupEntry(group, states[final].speedIndex), load.food, load.wood,
			                                    townNeeds, speed));
		}
	}
	// with the food speed-up on: speed x the info's foodPowerupIncrease ((inferred) the matching field of openblack's
	// GVillagerInfo), kept at float precision; then truncated
	int32_t whole = static_cast<int32_t>(speed);
	if (foodSpeedUpApplies && villager->foodSpeedUp != 0)
	{
		whole = static_cast<int32_t>(speed * info->foodPowerupIncrease);
	}
	// SetVillagerSpeed(whole, true), with the factor
	SetVillagerSpeed(entity, whole, true);
}

void SetVillagerSpeed(entt::entity entity, int32_t whole, bool applyFactor)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* villager = registry.TryGet<const Villager>(entity);
	auto* wallHug = registry.TryGet<WallHug>(entity);
	const auto* info = VillagerInfoOf(entity);
	if (villager == nullptr || wallHug == nullptr || info == nullptr)
	{
		return;
	}
	// f = 1; without applyFactor it stays
	float f = 1.0f;
	if (applyFactor)
	{
		// the factor of the villager (its creation index), age, and for adults food, life and sex. The creation index is
		// a signed int in the original's multiplication
		const auto index = static_cast<int32_t>(std::max<int64_t>(object_index::Of(entity), 0));
		f = static_cast<float>((index * 47) % 31 - 16) * 0.01f + 1.0f;
		// the age, compared unsigned with grownUpAge and oldAge; the differences are loaded as unsigned, times 0.2 and 0.1,
		// at most 0.4
		const uint32_t age = villager::AgeFromBirthTurn(villager->birthTurn, CurrentGameTurn());
		const uint32_t grownUp = info->grownUpAge;
		const uint32_t old = info->oldAge;
		if (age < grownUp)
		{
			f -= std::min(static_cast<float>(grownUp - age) * 0.2f * 0.1f, 0.4f);
		}
		else if (age > old)
		{
			f -= std::min(static_cast<float>(age - old) * 0.2f * 0.1f, 0.4f);
		}
		else
		{
			// the desire for food (1 - min(food, 1)^3) * 0.1
			f -= villager::GetDesireForFood(entity) * 0.1f;
			// life * 0.1, and 0.2 more for a woman (Villager::sex)
			f -= villager->life * 0.1f;
			if (villager->sex == Villager::Sex::FEMALE)
			{
				f -= 0.2f;
			}
		}
	}
	// whole x f, truncated
	const auto raw = std::clamp(static_cast<int32_t>(static_cast<float>(whole) * f), 0, 0xFFFF);
	// clamped to 0..0xFFFF and stored as a u16: the distance per turn in map units. openblack keeps the speed in metres,
	// so it converts here; (inferred) the original never converts this value, it adds it to map coordinates, and
	// ToMetres (the conversion it uses everywhere else) is the nearest thing to it (it differs from / 6553.6f by at most
	// one bit)
	wallHug->speed = map_coords::ToMetres(raw);
}

void SetVillagerSpeedInMetres(entt::entity villager, float metres)
{
	// ConvertMetersToWholeDistance (m / 10 x 65536, truncated toward zero), then SetVillagerSpeed(whole, false)
	SetVillagerSpeed(villager, gutils::ConvertMetersToWholeDistance(metres), false);
}

uint16_t WholeSpeed(const WallHug& wallHug)
{
	return static_cast<uint16_t>(std::clamp(gutils::ConvertMetersToWholeDistance(wallHug.speed), 0, 0xFFFF));
}

float VillagerSpeedInMetres(entt::entity villager)
{
	// ConvertWholeDistanceToMeters of the u16: WallHug::speed holds ToMetres of that u16, the same single rounding of
	// whole x 10 / 65536
	const auto* wallHug = Locator::entitiesRegistry::value().TryGet<const WallHug>(villager);
	return wallHug != nullptr ? wallHug->speed : 0.0f;
}

float VillagerScaleForAge(const GVillagerInfo& info, uint32_t age)
{
	// the initial scale, then the scale for the age from it: GameFloatRand, the game's synced draws in the constructor's
	// order
	return villager::ScaleForAge(info, age, villager::InitialScaleForAge(info, age));
}

} // namespace openblack::ecs
