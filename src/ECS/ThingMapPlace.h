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
/// the thing is drawn, or because metres cannot keep every map position far from the corner of the map. A thing that the
/// game put at an exact map position keeps that position while it stays there. Any other thing is where it stands. A
/// thing with no place is at the corner of the map.
[[nodiscard]] map_coords::MapCoords Of(const Registry& registry, entt::entity thing);

/// Gives a thing the exact map position that the game puts it at, where its metres are now. A walker's walk and an
/// animal's turn also start from this map position. A thing with no metres gets nothing.
void Keep(Registry& registry, entt::entity thing, const map_coords::MapCoords& place);

/// Puts a thing at an exact map position: its metres become those of the map position, at the given height, and the
/// thing keeps the map position (see Keep)
void Put(Registry& registry, entt::entity thing, const map_coords::MapCoords& place, float height);

} // namespace openblack::ecs::thing_map_place
