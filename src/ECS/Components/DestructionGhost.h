/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/core/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "ECS/DestructionGhostRules.h"

namespace openblack::ecs::components
{

/// A ghost of a thing that has just gone (a building an effect destroyed, a tree given to a store or a site, a pot poured
/// out of the hand). It breaks up where it stood for half a second. First the pattern that moves and thins adds its
/// colour and lays the depth. Then the thing draws in its own materials where that depth is.
struct DestructionGhost
{
	entt::id_type mesh {0};
	glm::mat4 model {1.0f};
	/// Of the 500 it lasts, in milliseconds of game time
	int millisecondsLeft {destruction_ghost::k_LifeMilliseconds};
	/// It has been drawn once, so the time drawing it takes runs down from the next frame
	bool shown {false};
};

/// The point a ghost takes its place by among the things that blend: the place of the thing it copies, as the game
/// keeps it with the copy of its matrix. It is not the middle of the model's box.
[[nodiscard]] inline glm::vec3 SortPoint(const DestructionGhost& ghost)
{
	return glm::vec3(ghost.model[3]);
}

} // namespace openblack::ecs::components
