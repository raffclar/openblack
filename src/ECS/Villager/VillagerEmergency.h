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
#include <glm/vec2.hpp>

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

// The villager side of a damaged building and of the town emergency (docs/bw1-notes/villagers.md "Repairs, the tap
// on a home and the town emergency"): the hit on an occupied home (SetStateWhenTappedOnAbode, 197
// AFTER_TAP_ON_ABODE), the state table's town emergency column (who answers the emergency call) and the congregation
// (242 / 243). The building / town side calls in: abodes::ReduceLife and a tap on the abode call
// SetStateWhenTappedOnAbode for every inhabitant; town_emergency::CallAllVillagersToTownEmergency calls
// CallToTownEmergency for every inhabitant of every abode of the town. The repairers themselves are
// the builders (VillagerBuild.h): no state of their own.

namespace openblack::ecs::villager
{
// ---- the hit on a home -------------------------------------------------------------------------------------------

/// Called for each inhabitant when the abode's new life is below 1 (ReduceLife) and when the abode is tapped: available
/// (IsAvailable) and inside its home -> SetupAfterTapOnAbode(FindPosOutsideAbode(none), 163). The original returns
/// 1 / 0; nobody reads it
void SetStateWhenTappedOnAbode(entt::entity villager);
/// (pos, s): SetState(PREVIOUS, s) (raw, no town modifier), SetupMoveToPos(pos, 197 AFTER_TAP_ON_ABODE), and sets
/// the after-tap flag
void SetupAfterTapOnAbode(entt::entity villager, glm::ivec2 pos, VillagerStates previous);
/// State 197 AFTER_TAP_ON_ABODE: PlayAnimThenSetState(PREVIOUS, 1) (the yawn clip
/// stays: 23 WAIT_FOR_ANIMATION, FINAL = the stored state); 1
uint32_t AfterTapOnAbode(components::LivingAction& action);

// ---- the town emergency ------------------------------------------------------------------------------------------

/// The town emergency answer of GetFinalState's row, from k_TownEmergencyReaction: Always -> true; PreviousState
/// (row 220 only) -> PREVIOUS != 0; an empty slot -> false (not called)
[[nodiscard]] bool ReactsToTownEmergency(entt::entity villager);
/// The per-villager body of CallAllVillagersToTownEmergency: ReactsToTownEmergency -> SetTopState(242)
/// (the current state's exit runs, e.g. a builder's ExitBuilding). No draw of its own
void CallToTownEmergency(entt::entity villager);
/// State 242 GOTO_CONGREGATE_IN_TOWN_AFTER_EMERGENCY: no town -> 0 (stays in 242); pos = the town's
/// GetCongregationPos + GetPosFromAngle(GameFloatRand(2 pi), CongregationDistance(adults + children));
/// SetupMoveToWithHug(pos, 243); 1
uint32_t GotoCongregateInTownAfterEmergency(components::LivingAction& action);
/// State 243 CONGREGATE_IN_TOWN_AFTER_EMERGENCY: in the emergency (IsInStateOfEmergency) GameRand(12) != 0 -> 242,
/// 0 -> 36 GO_HOME; else (also without a town) GameRand(5) != 0 -> 242, 0 -> 163; through PlayAnimThenSetState (one
/// clip a stop); 1
uint32_t CongregateInTownAfterEmergency(components::LivingAction& action);
/// k = n x 0.025f, 1 when not below 1; (float)(k x 20 + 10), in single-precision steps: 10 m for an empty town, 30 m
/// from 40 people on
[[nodiscard]] float CongregationDistance(uint32_t townPopulation);
} // namespace openblack::ecs::villager
