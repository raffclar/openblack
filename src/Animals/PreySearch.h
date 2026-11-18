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

#include <optional>
#include <vector>

#include <glm/common.hpp>
#include <glm/vec2.hpp>

#include "3D/MapCoords.h"

// A hungry hunter looks for prey in the map cells of a spiral around its own cell. It takes the first prey in the first
// cell that has one. SpiralOrder gives the place of each cell in that order, so that the hunter looks only at the
// living in those cells and not at all the living of the land. Pure, tested on its own.

namespace openblack::animals::prey_search
{

/// The cells of a spiral search from a centre cell, with the place of each cell in the order of the search
class SpiralOrder
{
public:
	/// The spiral looks at `cells` cells: the centre first, then each step of map_coords::Spiral
	SpiralOrder(glm::ivec2 centre, uint32_t cells)
	{
		std::vector<glm::ivec2> order;
		order.reserve(cells);
		auto cell = centre;
		map_coords::Spiral spiral;
		for (uint32_t i = 0; i < cells; ++i)
		{
			order.push_back(cell);
			const auto& step = spiral.Next();
			cell += glm::ivec2(step.x, step.z);
		}
		if (order.empty())
		{
			return;
		}
		_min = order.front();
		auto max = order.front();
		for (const auto& each : order)
		{
			_min = glm::min(_min, each);
			max = glm::max(max, each);
		}
		_width = max.x - _min.x + 1;
		_height = max.y - _min.y + 1;
		_ranks.assign(static_cast<size_t>(_width) * static_cast<size_t>(_height), k_NotInSpiral);
		for (uint32_t i = 0; i < order.size(); ++i)
		{
			_ranks[IndexOf(order[i])] = i;
		}
	}

	/// The place of a cell in the order of the search, or none when the search does not look at the cell
	[[nodiscard]] std::optional<uint32_t> RankOf(glm::ivec2 cell) const
	{
		if (cell.x < _min.x || cell.y < _min.y || cell.x >= _min.x + _width || cell.y >= _min.y + _height)
		{
			return std::nullopt;
		}
		const auto rank = _ranks[IndexOf(cell)];
		return rank == k_NotInSpiral ? std::nullopt : std::optional<uint32_t>(rank);
	}

private:
	static constexpr uint32_t k_NotInSpiral = UINT32_MAX;

	[[nodiscard]] size_t IndexOf(glm::ivec2 cell) const
	{
		return static_cast<size_t>(cell.y - _min.y) * static_cast<size_t>(_width) + static_cast<size_t>(cell.x - _min.x);
	}

	glm::ivec2 _min {0, 0};
	int32_t _width {0};
	int32_t _height {0};
	std::vector<uint32_t> _ranks;
};

} // namespace openblack::animals::prey_search
