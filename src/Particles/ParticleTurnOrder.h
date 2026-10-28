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

#include <ranges>
#include <span>
#include <vector>

/// The order in which the turn steps the running particle effects that no miracle steps. The game keeps its spot
/// visuals in a list that puts each new one at its head, so the turn steps the newest spot visual first. The other
/// effects keep the order in which they started. A spot visual draws its random numbers from the seed that every
/// computer of a network game shares, so its order changes the world.
namespace openblack::particles::turn_order
{

/// A running effect, in the order of its start
struct Entry
{
	uint32_t id;
	bool spotVisual;
};

/// The ids in the order of their steps: first the effects that are not spot visuals, oldest first, then the spot
/// visuals, newest first
[[nodiscard]] inline std::vector<uint32_t> Steps(std::span<const Entry> started)
{
	std::vector<uint32_t> steps;
	steps.reserve(started.size());
	for (const auto& entry : started)
	{
		if (!entry.spotVisual)
		{
			steps.push_back(entry.id);
		}
	}
	for (const auto& entry : std::ranges::reverse_view(started))
	{
		if (entry.spotVisual)
		{
			steps.push_back(entry.id);
		}
	}
	return steps;
}

} // namespace openblack::particles::turn_order
