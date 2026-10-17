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

#include "3D/MapCoords.h"

namespace openblack::ecs
{
class Registry;
}

/// Where a thing is, as the map keeps it
namespace openblack::ecs::thing_map_place
{

/// Where a thing is, as a map position with altitude 0. A thing that moves in map units has its place in map units: a
/// flock, an animal, a firefly, and a walker where its walk holds it. Their metres are not used, because they are where
/// the thing is drawn, or because metres cannot keep every map position far from the corner of the map. Any other thing
/// is where it stands. A thing with no place is at the corner of the map.
[[nodiscard]] map_coords::MapCoords Of(const Registry& registry, entt::entity thing);

} // namespace openblack::ecs::thing_map_place
