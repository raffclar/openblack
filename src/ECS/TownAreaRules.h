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

#include <limits>

#include "3D/MapCoords.h"
#include "ECS/Components/Town.h"

/// The ground a town covers: a box in map units about its buildings and fields, each widened by its radius. The town
/// keeps the box and makes it again only when a building joins or leaves it, or when one of its fields goes. A field
/// that the town gets later does not widen the box until then.
namespace openblack::ecs::town_area
{

/// The box before anything widens it: the least corner at the far end of the map, the greatest at 0
[[nodiscard]] constexpr components::TownArea Emptied()
{
	return {.min = {std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::max()}, .max = {0, 0}};
}

/// A building or a field at a place widens the box by its radius on each side. Each step is a float step, in the order
/// that the game uses, and each new corner is cut to whole map units.
void Widen(components::TownArea& area, const map_coords::MapCoords& at, float radius);

/// The middle of the box, the town's centre for its scenic forest
[[nodiscard]] map_coords::MapCoords Centre(const components::TownArea& area);

/// The radius of the town: half of the longer side of the box, each side first cut to whole metres
[[nodiscard]] float Radius(const components::TownArea& area);

} // namespace openblack::ecs::town_area
