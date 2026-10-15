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

#include <array>
#include <optional>

#include "Creature/CreatureDesires.h"

/// The rules of a creature's mind turn that come before it decides what to do, and the extras when a plan ends well
namespace openblack::creature_turn_order
{
/// Before it grows up to the first stage, the desires of its body stay at 0 at the start of each turn. Each turn adds
/// only one turn of growth to them, so the creature does not act on them.
constexpr uint32_t k_FirstStageWithBody = 1;
void HoldBodyDesires(creature_desires::Desires& desires, const std::array<bool, creature_desires::k_DesireCount>& body,
                     uint32_t phase);

/// After the hand let go of it, the first turn that finds a plan with steps to do starts the agenda of that plan again
/// from its first step. The mark that the hand held it stays until then.
[[nodiscard]] bool RestartsAfterHold(bool heldSinceRestart, bool planHasSteps);

/// A plan with steps to do that serves a desire other than the desire to follow the player counts as a plan of its own
[[nodiscard]] bool OwnPlanRuns(bool planHasSteps, creature_desires::Desire desire);

/// The creature stops copying the player when this many seconds pass with no plan of its own since it last stopped
/// what it did. It then does not follow the player for a time.
constexpr float k_CopyTimeoutSeconds = 15.0f;
constexpr float k_NoFollowAfterCopyTimeoutSeconds = 60.0f;
/// One turn of the copy timeout. The count grows on each turn with no plan of its own, and it does not go back to 0
/// when a plan starts. It goes back to 0 only when the timeout occurs. Returns true on the turn the timeout occurs.
[[nodiscard]] bool StepCopyTimeout(uint32_t& turnsWithoutOwnPlan, bool ownPlanSinceStop, uint32_t wholeTurnsPerSecond);

/// What the end of a plan that the creature did to its end does, apart from the cost to its body
struct Completion
{
	/// The plan counts for the urge of its desire and for the rules of that desire
	bool counts {false};
	/// The life that the creature gets, if the plan changes its life
	std::optional<float> life;
	/// The temperature of the creature becomes the temperature of the climate at its position
	bool climateTemperature {false};
};
/// A forced plan counts only when the order that forced it said so. A plan for the desire to restore health adds half
/// of the full life. An action that makes the creature recover fully gives full life and the temperature of the climate.
constexpr float k_RestoreHealthLife = 0.5f;
constexpr float k_FullLife = 1.0f;
[[nodiscard]] Completion CompletionOf(creature_desires::Desire desire, bool forced, bool forcedCounts, bool fullRecovery,
                                      float life);
} // namespace openblack::creature_turn_order
