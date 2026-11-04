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

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "3D/MapCoords.h"

namespace openblack::ecs
{
class Registry;
}

/// The destination of a living thing as the scripts read it (GET_OBJECT_DESTINATION): a map position with a height
/// above the land. The game keeps one such place for each living thing:
/// - A villager: the goal of its walk, and the height of the goal above the land.
/// - An animal: the goal of its move, and the height above the land that it wants to have there.
/// - A creature: the place that its walks set (see CreatureLocomotion::scriptDestination).
/// Any other thing has no destination.
namespace openblack::ecs::script_destination
{

/// A point that a creature walks to as its destination: the map position of the point, with no height
[[nodiscard]] constexpr map_coords::MapCoords OfPoint(glm::vec2 point)
{
	return {.x = map_coords::ToFixed(point.x), .z = map_coords::ToFixed(point.y), .altitude = 0.0f};
}

/// The destination of a thing, none for a thing that does not live
[[nodiscard]] std::optional<map_coords::MapCoords> Of(const Registry& registry, entt::entity thing);

} // namespace openblack::ecs::script_destination
