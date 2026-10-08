/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerAge.h"

#include <bit>

#include <fmt/format.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerBirth.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "InfoConstants.h"
#include "Locator.h"

// A villager's age, scale, old age and pregnancy (VillagerAge.h)

namespace openblack::ecs::villager
{
using namespace components;

namespace
{
/// An adult is at least 18
constexpr uint32_t k_AdultAge = 18;
/// An adult's initial scale
constexpr float k_AdultInitialScale = 0.9f;
/// A child grows up to 0.75 of the way to the next age's scale
constexpr float k_ChildScaleStep = 0.75f;
/// An adult's scale: (0.05 - GameFloatRand(0.1)) + 1
constexpr float k_AdultScaleRand = 0.1f;
constexpr float k_AdultScaleBase = 0.05f;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// ageToScale[i]; i = -1 reads the field before the table (dancingSpeed) as a float, as the original does at
/// age 0
float AgeToScaleAt(const GVillagerInfo& info, int32_t i)
{
	if (i < 0)
	{
		return std::bit_cast<float>(info.dancingSpeed);
	}
	const auto& table = info.ageToScale.values;
	// (openblack, guard) past the 20 floats the original reads the next fields: never for an age below grownUpAge 13
	return static_cast<size_t>(i) < table.size() ? table.at(static_cast<size_t>(i)) : table.back();
}

float ScaleOf(entt::entity villager)
{
	const auto* t = Entities().TryGet<const Transform>(villager);
	return t != nullptr ? t->scale.x : 1.0f;
}

void SetScale(entt::entity villager, float scale)
{
	// The uniform scale
	if (auto* t = Entities().TryGet<Transform>(villager))
	{
		t->scale = glm::vec3(scale);
	}
}
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

bool GrownUp(uint32_t age, const GVillagerInfo& info)
{
	return !(age < info.grownUpAge);
}

uint32_t GrownUpAge(const GVillagerInfo& info)
{
	return info.grownUpAge < k_AdultAge ? k_AdultAge : info.grownUpAge;
}

bool RescaleTurn(uint32_t turn)
{
	// The game turn % (a quarter of the year), unsigned
	return turn % (k_TurnsPerYear >> 2u) == 0;
}

uint32_t OldAgeRange(float r, uint32_t oldAge, uint32_t retirementAge)
{
	// r x r x r, each product rounded to a float
	const float r2 = r * r;
	const float r3 = r2 * r;
	// The unsigned span (retirement - old) as a float, times r^3, truncated
	const auto span = static_cast<float>(static_cast<double>(retirementAge - oldAge));
	return static_cast<uint32_t>(map_coords::FtoL(r3 * span));
}

bool OldAgeDies(uint32_t age, uint32_t d, uint32_t retirementAge)
{
	return age + d > retirementAge;
}

float InitialScaleForAge(const GVillagerInfo& info, uint32_t age)
{
	// A child: ageToScale[age - 1]; else 0.9
	if (age < info.grownUpAge)
	{
		return AgeToScaleAt(info, static_cast<int32_t>(age) - 1);
	}
	return k_AdultInitialScale;
}

float ScaleForAge(const GVillagerInfo& info, uint32_t age, float current)
{
	if (age < info.grownUpAge)
	{
		// A random part of 0.75 of the gap to the next age's scale, added to the current scale
		const float gap = AgeToScaleAt(info, static_cast<int32_t>(age) + 1) - current;
		const float range = gap * k_ChildScaleStep;
		const float r = GameFloatRand(range);
		return current + r;
	}
	// t = (0.05 - GameFloatRand(0.1)) + 1
	const float r1 = GameFloatRand(k_AdultScaleRand);
	const float t = (k_AdultScaleBase - r1) + 1.0f;
	// Below t -> another (0.05 - GameFloatRand(0.1)) + 1; else the current scale
	if (current < t)
	{
		const float r2 = GameFloatRand(k_AdultScaleRand);
		return (k_AdultScaleBase - r2) + 1.0f;
	}
	return current;
}

// ---- the villager ------------------------------------------------------------------------------------------------

void InitialiseScale(entt::entity villager, uint32_t age)
{
	SetScale(villager, InitialScaleForAge(InfoOf(villager), age));
}

void SetScaleForAge(entt::entity villager, uint32_t age)
{
	SetScale(villager, ScaleForAge(InfoOf(villager), age, ScaleOf(villager)));
}

uint32_t SetAgeAndScale(entt::entity villager, uint32_t age)
{
	const auto& info = InfoOf(villager);
	const uint32_t turn = CurrentTurn();
	return SetAge(villager, info, age, turn, [villager, &info](uint32_t set) {
		// The meshes only when the age crosses grownUpAge (GetAge with the old birth turn): a new child the child
		// meshes, a new adult the adult detail meshes. (pending) the skeleton branch is not here
		const uint32_t old = GetAge(villager);
		const bool child = set < info.grownUpAge;
		if (child && !(old < info.grownUpAge))
		{
			SetVillagerMeshes(villager, info, true, false);
		}
		else if (!child && old < info.grownUpAge)
		{
			SetVillagerMeshes(villager, info, false, false);
		}
		InitialiseScale(villager, set);
		SetScaleForAge(villager, set);
	});
}

uint32_t CheckChildGrownUp(entt::entity villager)
{
	auto* v = Entities().TryGet<Villager>(villager);
	if (v == nullptr)
	{
		return 0;
	}
	const auto& info = InfoOf(villager);
	const uint32_t age = GetAge(villager);
	if (GrownUp(age, info))
	{
		// No longer a child
		v->flags = static_cast<uint16_t>(v->flags & ~Villager::k_FlagChild);
		// SetAge(max(grownUpAge, 18))
		const uint32_t to = GrownUpAge(info);
		SetAgeAndScale(villager, to);
		if (TraceOn(villager))
		{
			Trace(villager, fmt::format("age: grown {} -> {} (abode {})", age, to,
			                            static_cast<uint32_t>(Entities().Get<const Villager>(villager).abode)));
		}
		// The abode's ChildToAdult, else the town's
		const auto* again = Entities().TryGet<const Villager>(villager);
		// (guard) the original reads the abode link unchecked: this stands for the abode's unlinking on deletion
		if (again != nullptr && again->abode != entt::null && ecs::IsAvailable(again->abode))
		{
			abode_villagers::ChildToAdult(again->abode, villager);
		}
		// (guard) the original reads the town link unchecked: this stands for the missing town unlinking
		else if (again != nullptr && again->town != entt::null && ecs::IsAvailable(again->town))
		{
			town_villagers::ChildToAdult(again->town, villager);
		}
		return ChildBecomesAdult(villager);
	}
	// Not grown up: rescale every 375 turns
	if (RescaleTurn(CurrentTurn()))
	{
		SetScaleForAge(villager, age);
		if (TraceOn(villager))
		{
			Trace(villager, fmt::format("age: rescale {} -> {:.4f}", age, ScaleOf(villager)));
		}
	}
	return 0;
}

uint32_t ChildBecomesAdult(entt::entity villager)
{
	if (auto* v = Entities().TryGet<Villager>(villager))
	{
		v->mother = entt::null;
	}
	// CheckNeedNewAbode, then 234 GO_HOME_AND_CHANGE; 1
	CheckNeedNewAbode(villager);
	SetTopState(villager, VillagerStates::GoHomeAndChange);
	return 1;
}

uint32_t ChildBecomesAdultState(LivingAction& action)
{
	return ChildBecomesAdult(Entities().ToEntity(action));
}

bool CheckDeathFromOldAge(entt::entity villager)
{
	const auto& info = InfoOf(villager);
	// Not past oldAge -> 0
	if (!(GetAge(villager) > info.oldAge))
	{
		return false;
	}
	// d = GameRand(OldAgeRange(GameFloatRand(1)))
	const float r = GameFloatRand(1.0f);
	const uint32_t n = OldAgeRange(r, info.oldAge, info.retirementAge);
	const uint32_t d = GameRand(n);
	// The age (read again) + d past retirementAge dies
	const uint32_t age = GetAge(villager);
	const bool dies = OldAgeDies(age, d, info.retirementAge);
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("age: old age {} r={:.6f} n={} d={} -> {}", age, r, n, d, dies ? "die" : "live"));
	}
	if (!dies)
	{
		return false;
	}
	// Dies of old age, by no player, with the info's life
	VillagerDead(villager, DeathReason::OldAge, std::nullopt, info.life, 1);
	return true;
}

bool IsPregnant(entt::entity villager)
{
	// A woman with a pregnancy count
	const auto* v = Entities().TryGet<const Villager>(villager);
	return v != nullptr && InfoOf(villager).sex == SexType::Female && v->pregnancy != 0;
}

uint32_t UpdatePregnancy(entt::entity villager)
{
	auto* v = Entities().TryGet<Villager>(villager);
	// Pregnant and not controlled by a script
	if (v == nullptr || !IsPregnant(villager) || script_held::IsControlledByScript(villager))
	{
		return 0;
	}
	// Count the pregnancy down by the turns since the last check (16 bits); still > 0 -> 0
	const auto turns = GetGameTurnsSinceLastChecked(villager, CurrentTurn());
	v->pregnancy = static_cast<int16_t>(static_cast<uint16_t>(v->pregnancy) - static_cast<uint16_t>(turns));
	if (v->pregnancy > 0)
	{
		return 0;
	}
	// Due: start giving birth (VillagerBirth.cpp)
	return HousewifeStartsGivingBirth(villager);
}
} // namespace openblack::ecs::villager
