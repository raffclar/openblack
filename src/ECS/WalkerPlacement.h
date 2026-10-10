/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs
{
class Registry;
}

/// Putting a walker somewhere else at once, as the game's tools do, so that its walk carries on from there
namespace openblack::ecs::walker_placement
{

/// Puts a thing at a place on the land. A walker's walk takes up its new place: where the walk holds it, and the step,
/// circle and way of going round that it had worked out from the old one are dropped. One that was on its way heads
/// for the same goal from there, working its way out afresh on its next turn, as when a walk is set up; one that had
/// arrived stays where it is put. Anything else only moves.
void Place(Registry& registry, entt::entity entity, glm::vec3 position);

} // namespace openblack::ecs::walker_placement
