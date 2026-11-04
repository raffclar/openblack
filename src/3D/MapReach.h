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

#include <algorithm>
#include <limits>

namespace openblack
{
/// The smallest box of fixed map points that holds all the points a calculation read the land height at
struct MapReach
{
	int32_t minX {std::numeric_limits<int32_t>::max()};
	int32_t minZ {std::numeric_limits<int32_t>::max()};
	int32_t maxX {std::numeric_limits<int32_t>::min()};
	int32_t maxZ {std::numeric_limits<int32_t>::min()};

	/// Makes the box large enough to hold the point
	void Add(int32_t x, int32_t z)
	{
		minX = std::min(minX, x);
		minZ = std::min(minZ, z);
		maxX = std::max(maxX, x);
		maxZ = std::max(maxZ, z);
	}

	[[nodiscard]] bool Empty() const { return minX > maxX || minZ > maxZ; }

	bool operator==(const MapReach&) const = default;
};
} // namespace openblack
