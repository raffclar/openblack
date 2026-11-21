/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <algorithm>
#include <vector>

namespace openblack::death_wave
{

/// The places of the villagers that a wave of deaths kills, from a list of this size: this many places, spread evenly
/// from the first place. All the places when the list has fewer. Each place is a different place, in order.
[[nodiscard]] inline std::vector<size_t> SpreadPicks(size_t size, size_t count)
{
	const auto picks = std::min(size, count);
	std::vector<size_t> places;
	places.reserve(picks);
	for (size_t i = 0; i < picks; ++i)
	{
		places.push_back((i * size) / picks);
	}
	return places;
}

} // namespace openblack::death_wave
