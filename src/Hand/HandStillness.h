/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

#include "ECS/Components/HandStillness.h"

/// The count of game turns that the player's hand stays in one place. A creature that looks at the camera stops when
/// the hand stays still for too long.
namespace openblack::hand_stillness
{

/// The hand moves when its added-up movement gets to this length
constexpr float k_MoveDistance = 1.0f;
/// The velocity of the hand goes this part of the way to the movement of the turn, made a velocity, each turn
constexpr float k_VelocityFollow = 0.6f;
/// The game turns in a second, which make the movement of a turn a velocity
constexpr float k_TurnsPerSecond = 10.0f;

/// One game turn for the hand. The game adds the movement since the last turn to the movement since the hand last
/// moved, as one vector, so movements that go back and cancel each other do not count. When the length of that vector
/// gets to 1, the count and the movement go back to zero. If not, the count increases by one. The velocity of the hand
/// follows the movement of the turn.
void Step(ecs::components::HandStillness& stillness, const glm::vec3& position) noexcept;

} // namespace openblack::hand_stillness
