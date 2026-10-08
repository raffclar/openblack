/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/fwd.hpp>

#include "ECS/Components/LivingAction.h"

namespace openblack
{
struct GVillagerInfo;
}

// A villager's age kept as its birth turn (docs/bw1-notes/villagers.md), and what the age does: the child that
// grows up, the scale for the age, the old age's death and the pregnancy's count down (UpdatePregnancy). The births are
// in VillagerBirth.h.
namespace openblack::ecs::villager
{

/// Game turns in a year
inline constexpr uint32_t k_TurnsPerYear = 1500;

/// (turn - birthTurn) / 1500, an unsigned division
[[nodiscard]] constexpr uint32_t AgeFromBirthTurn(int32_t birthTurn, uint32_t turn)
{
	return (turn - static_cast<uint32_t>(birthTurn)) / k_TurnsPerYear;
}

/// birthTurn = turn - age * 1500
[[nodiscard]] constexpr int32_t BirthTurnForAge(uint32_t age, uint32_t turn)
{
	return static_cast<int32_t>(turn - age * k_TurnsPerYear);
}

// ---- the pure layer ----------------------------------------------------------------------------------------------

/// GetAge >= grownUpAge, unsigned
[[nodiscard]] bool GrownUp(uint32_t age, const GVillagerInfo& info);
/// The age a child grows up to: max(grownUpAge, 18)
[[nodiscard]] uint32_t GrownUpAge(const GVillagerInfo& info);
/// The rescale turn of a child: turn % (a quarter of the year = 375) == 0
[[nodiscard]] bool RescaleTurn(uint32_t turn);
/// n = the truncated r^3 x (retirementAge - oldAge) (float steps), the old age's GameRand range
[[nodiscard]] uint32_t OldAgeRange(float r, uint32_t oldAge, uint32_t retirementAge);
/// age + d > retirementAge (unsigned) -> dies
[[nodiscard]] bool OldAgeDies(uint32_t age, uint32_t d, uint32_t retirementAge);
/// A child (age < grownUpAge) ageToScale[age - 1] (at age 0 the field before the table, dancingSpeed, read as a
/// float), an adult 0.9
[[nodiscard]] float InitialScaleForAge(const GVillagerInfo& info, uint32_t age);
/// From the scale `current`: a child current + GameFloatRand((ageToScale[age + 1] - current) x 0.75); an adult
/// t = (0.05 - GameFloatRand(0.1)) + 1, current < t ? (0.05 - GameFloatRand(0.1)) + 1 : current. Draws the game's
/// synced GameFloatRand
[[nodiscard]] float ScaleForAge(const GVillagerInfo& info, uint32_t age, float current);

// ---- the villager ------------------------------------------------------------------------------------------------

/// The periodic check, children only: grown up -> no longer a child, SetAge(max(13, 18)) (no mesh change, the age
/// is already >= 13), the abode's ChildToAdult (or the town's), and ChildBecomesAdult (its result); else on a
/// rescale turn SetScaleForAge(GetAge()). The result
uint32_t CheckChildGrownUp(entt::entity villager);
/// mother = none; CheckNeedNewAbode; SetTopState(234 GO_HOME_AND_CHANGE); 1
uint32_t ChildBecomesAdult(entt::entity villager);
/// State 115 CHILD_BECOMES_ADULT: ChildBecomesAdult (only a script sets 115 (inferred): no other setter was searched)
uint32_t ChildBecomesAdultState(components::LivingAction& action);
/// SetAge after the creation: VillagerCore's SetAge with the original's meshes (only when the age crosses
/// grownUpAge) and InitialiseScale + SetScaleForAge
uint32_t SetAgeAndScale(entt::entity villager, uint32_t age);
/// InitialScaleForAge / ScaleForAge on the villager's Transform scale (the uniform scale)
void InitialiseScale(entt::entity villager, uint32_t age);
void SetScaleForAge(entt::entity villager, uint32_t age);
/// age > oldAge (60) -> r = GameFloatRand(1), d = GameRand(OldAgeRange(r)); dies -> VillagerDead(9 OLD_AGE, no
/// player, info.life, 1); 1. Else 0
bool CheckDeathFromOldAge(entt::entity villager);
/// A woman (info sex == 1) with a pregnancy count != 0
[[nodiscard]] bool IsPregnant(entt::entity villager);
/// Pregnant and not controlled by a script: the pregnancy count -= GetGameTurnsSinceLastChecked (16 bits); <= 0 ->
/// HousewifeStartsGivingBirth (its result). Else 0
uint32_t UpdatePregnancy(entt::entity villager);
/// HousewifeStartsGivingBirth, UpdatePregnancy's tail, is in VillagerBirth.h
} // namespace openblack::ecs::villager
