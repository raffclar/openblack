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

#include <algorithm>
#include <array>
#include <optional>

#include <glm/vec3.hpp>

#include "Common/GUtilsDistance.h"
#include "Enums.h"

/// The rules of a villager under a script's control: playing the clips a script asks for, the states that may take it
/// out of the script's hands, and how scripts measure distance and speed
namespace openblack::ecs::villager_script_rules
{

/// A script's distance below this is no distance at all
constexpr float k_NoDistance = 0.5f;

/// The distance a script measures between two points: along the ground only, and none at all under half a metre, so a
/// script waiting for something to reach a place sees it there once it has stepped onto it
[[nodiscard]] inline float ScriptDistance(const glm::vec3& from, const glm::vec3& to)
{
	const float distance = gutils::Hypotenuse(to.x - from.x, to.z - from.z);
	return distance < k_NoDistance ? 0.0f : distance;
}

/// A speed a script gives in metres a turn, as the wall-hugging walk holds speeds: the game keeps it in whole map units a
/// turn, which the walk holds in hundredths of the map units of one metre a second
[[nodiscard]] constexpr float ScriptSpeedToWalkSpeed(float metresPerTurn)
{
	if (metresPerTurn <= 0.0f)
	{
		return 0.0f;
	}
	constexpr int32_t k_MaxWholeSpeed = 0xffff;
	constexpr float k_WholePerWalkUnit = static_cast<float>(0x10000) * 0.01f;
	const auto whole = std::min(gutils::ConvertMetersToWholeDistance(metresPerTurn), k_MaxWholeSpeed);
	return static_cast<float>(whole) / k_WholePerWalkUnit;
}

/// The walk's speed back in metres a turn, as a script reads it
[[nodiscard]] constexpr float WalkSpeedToScriptSpeed(float walkSpeed)
{
	constexpr float k_WholePerWalkUnit = static_cast<float>(0x10000) * 0.01f;
	return gutils::ConvertWholeDistanceToMeters(static_cast<int32_t>(walkSpeed * k_WholePerWalkUnit));
}

/// One turn of playing a script's clip: the plays left after it, and the state to go into once the clip playing ends
struct ScriptClipStep
{
	uint32_t playsLeft;
	std::optional<VillagerStates> then;
};

/// A villager playing a script's clip counts one more play each time it starts the clip: with more to come it plays it
/// again, after the last it stands waiting for the script. With no plays left it does nothing more; a script asking for
/// endless plays gives a count that never runs out
[[nodiscard]] constexpr ScriptClipStep StepScriptClip(uint32_t playsLeft)
{
	if (playsLeft == 0)
	{
		return {.playsLeft = 0, .then = std::nullopt};
	}
	--playsLeft;
	return {.playsLeft = playsLeft, .then = playsLeft == 0 ? VillagerStates::InScript : VillagerStates::ScriptPlayAnim};
}

/// Whether a villager put into a state by a script carries on with the walk it was on: only the walking states walk.
/// Put into any other, the villager stops where it is, as its new state takes no steps.
[[nodiscard]] constexpr bool KeepsWalking(VillagerStates state)
{
	return state == VillagerStates::MoveToPos || state == VillagerStates::MoveToObject ||
	       state == VillagerStates::MoveOnStructure;
}

/// Whether a villager has played the clip a script asked for: not while a clip plays out, nor while plays are left
[[nodiscard]] constexpr bool HasPlayedScriptClip(VillagerStates top, uint32_t playsLeft)
{
	if (top == VillagerStates::WaitForAnimation)
	{
		return false;
	}
	if (top == VillagerStates::ScriptPlayAnim)
	{
		return playsLeft == 0;
	}
	return true;
}

/// The states that hand a villager back to its script the same way when it leaves them
constexpr std::array k_ScriptHeldStates = {VillagerStates::InScript, VillagerStates::MoveAlongPath,
                                           VillagerStates::ScriptGoAndMoveAlongPath};

/// Whether two states let go of a villager the same way: both held by a script, or both playing a script's clip
[[nodiscard]] constexpr bool LeaveTheSameWay(VillagerStates a, VillagerStates b)
{
	const auto held = [](VillagerStates state) {
		return std::ranges::find(k_ScriptHeldStates, state) != k_ScriptHeldStates.end();
	};
	if (held(a) && held(b))
	{
		return true;
	}
	return a == VillagerStates::ScriptPlayAnim && b == VillagerStates::ScriptPlayAnim;
}

/// What a state says about a villager a script holds
struct StateRules
{
	bool isScriptState;
	bool isScriptInterruptable;
};

/// Whether a villager a script holds may leave for another state: only for another of the script's states, one the
/// script lets interrupt it, being picked up, or a state it leaves the same way as the one it works towards
[[nodiscard]] constexpr bool ScriptLetsGo(VillagerStates finalState, VillagerStates next, StateRules nextRules)
{
	return nextRules.isScriptState || nextRules.isScriptInterruptable || next == VillagerStates::InHand ||
	       LeaveTheSameWay(finalState, next);
}

} // namespace openblack::ecs::villager_script_rules
