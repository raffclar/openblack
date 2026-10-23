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

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

namespace openblack::ecs
{
class Registry;
}

/// Where a creature's home is
namespace openblack::ecs::creature_home
{

/// A creature's home: its player's temple's pen while the temple stands, else the home its leash keeps it at; none
/// without either
[[nodiscard]] std::optional<glm::vec3> HomeOf(const Registry& registry, entt::entity creature);
/// The same home as a map position: the temple's pen as the map keeps it, else the exact map position that the leash
/// keeps; none without a home
[[nodiscard]] std::optional<map_coords::MapCoords> HomeOnMapOf(const Registry& registry, entt::entity creature);
/// The pen of the first temple of the player whose mesh marks a pen; none when no temple of the player has one
[[nodiscard]] std::optional<glm::vec3> TemplePenOf(const Registry& registry, PlayerNames owner);
/// The creature home of the player's citadel on the map: a marked place of the first temple of the player, on the ground.
/// None when the player has no temple.
[[nodiscard]] std::optional<map_coords::MapCoords> CitadelCreatureHomeOf(const Registry& registry, PlayerNames owner);

} // namespace openblack::ecs::creature_home
