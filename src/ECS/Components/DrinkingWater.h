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

#include "3D/MapCoords.h"

namespace openblack::ecs::components
{

/// Where a building's people take their flock to drink: found when the building is built, and looked for again,
/// further out, by a shepherd from a home that had none
struct DrinkingWater
{
	std::optional<map_coords::MapCoords> place;
};

} // namespace openblack::ecs::components
