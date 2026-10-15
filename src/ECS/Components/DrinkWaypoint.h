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

namespace openblack::ecs::components
{

/// A place that the land script marks for drinking. It has a place and nothing else, and it is never drawn.
struct DrinkWaypoint
{
	/// The order in which the land made the water points, from 0
	uint32_t number {0};
};

} // namespace openblack::ecs::components
