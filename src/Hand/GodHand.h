/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <array>
#include <optional>
#include <span>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/HandStillness.h"
#include "Hand/HandStillness.h"

/// The hand of a god that no person plays, as the world has it. Every god has such a hand, in a game alone and in a
/// network game. A god with no computer mind has one too.
/// - No person reports the hand, so the input of each turn moves the hand to the origin of the world.
/// - In a turn in which the computer mind of the god does a step of its action, the mind then puts the hand where it
///   was before the step.
/// - The minds take their turns before the creatures. Thus in such a turn a creature reads the hand of the mind. In all
///   other turns it reads the origin of the world.
/// - The citadel of the god puts the hand at the citadel when the land loads. The input of the first turn moves the
///   hand from there.
namespace openblack::god_hand
{

/// Where the input of a turn puts the hand of a god that no person plays
constexpr glm::vec3 k_NoReport {0.0f};

/// The input of a turn: the hand goes to the origin of the world, and its stillness and velocity count the movement
/// from where the world had it (see hand_stillness::Step)
inline void InputTurn(ecs::components::HandStillness& hand) noexcept
{
	hand_stillness::Step(hand, k_NoReport);
}

/// The computer mind puts the hand where it was before the step of the turn. The stillness and the velocity do not
/// change until the input of the next turn.
inline void MindReport(ecs::components::HandStillness& hand, const glm::vec3& place) noexcept
{
	hand.last = place;
}

/// The places of the two hands of a player, as the world reads them
using Hands = std::array<std::optional<glm::vec3>, 2>;

/// The hands of a player as the creatures read them. A person's player has the hands that its reports give. Every other
/// god has one hand, where the world has it (see InputTurn and MindReport), the origin of the world too. A player that
/// the world does not have has no hands.
[[nodiscard]] constexpr Hands HandsOf(bool person, const Hands& reported, const std::optional<glm::vec3>& god) noexcept
{
	if (person)
	{
		return reported;
	}
	return {god, std::nullopt};
}

/// The hand nearest to a point, in metres: of hands at the same distance, the first. None when there is no hand.
[[nodiscard]] inline std::optional<glm::vec3> NearestOf(std::span<const std::optional<glm::vec3>> hands,
                                                        const glm::vec3& point) noexcept
{
	std::optional<glm::vec3> nearest;
	for (const auto& hand : hands)
	{
		if (hand.has_value() && (!nearest.has_value() || glm::distance(*hand, point) < glm::distance(*nearest, point)))
		{
			nearest = hand;
		}
	}
	return nearest;
}

} // namespace openblack::god_hand
