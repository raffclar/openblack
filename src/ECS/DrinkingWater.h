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

#include "3D/MapCoords.h"

/// Water to drink on the land: the nearest river point, or dry coast, near a place
namespace openblack::ecs::drinking_water
{

/// The nearest drinking water within reach (metres) of a place, on the land as it is now
[[nodiscard]] std::optional<map_coords::MapCoords> Find(const map_coords::MapCoords& from, float reach);
/// A building looks for its drinking water within reach and keeps what it finds, or keeps none
void FindFor(entt::entity building, float reach);

} // namespace openblack::ecs::drinking_water
