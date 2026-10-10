/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/vec2.hpp>

#include "ECS/Components/WallHug.h"

/// When a walker is where it was going, as the game decides it: positions are whole map units (a 65536th of ten
/// metres), as the walk holds them. Pure.
namespace openblack::ecs::walk_arrival
{

/// Whether a walker is closer to a point than the step it makes in a turn at its speed (metres a second)
[[nodiscard]] bool WithinAStep(glm::ivec2 position, glm::ivec2 point, float metresPerSecond);

/// Whether a walk to a goal is over for the state waiting on it. A walker within a step of its goal takes its last step
/// onto it on the next turn: the walk is over once it stands on its goal, not when it first comes within the step. A
/// walker with no walk under way (no state) is done.
[[nodiscard]] bool WalkIsOver(std::optional<components::MoveState> state, glm::ivec2 position, glm::ivec2 goal);

} // namespace openblack::ecs::walk_arrival
