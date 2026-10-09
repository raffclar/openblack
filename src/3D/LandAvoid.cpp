/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandAvoid.h"

#include <algorithm>
#include <array>
#include <utility>

using namespace openblack;
using namespace openblack::land_avoid;

namespace
{
/// Steps of the working out: a walkable cell not reached yet, one waiting to be walked from, and a blocked cell found
/// next to reached land
constexpr uint8_t k_Walkable = 4;
constexpr uint8_t k_Queued = 3;
constexpr uint8_t k_AvoidNextToLand = 5;

/// The neighbours a walk steps to, in the order it tries them
constexpr std::array<glm::ivec2, 4> k_Neighbours {glm::ivec2(-1, 0), glm::ivec2(0, -1), glm::ivec2(1, 0), glm::ivec2(0, 1)};

struct Cells
{
	int32_t size;
	std::vector<uint8_t>& values;

	[[nodiscard]] bool Contains(glm::ivec2 cell) const { return cell.x >= 0 && cell.y >= 0 && cell.x < size && cell.y < size; }
	[[nodiscard]] uint8_t& operator[](glm::ivec2 cell) const
	{
		return values[(static_cast<size_t>(cell.y) * static_cast<size_t>(size)) + static_cast<size_t>(cell.x)];
	}
};

/// Walks from the seed over the walkable cells, which become land, marking the blocked cells beside them; then the
/// cells not reached are out of reach
void Walk(const Cells& cells, glm::ivec2 seed)
{
	if (!cells.Contains(seed) || cells[seed] != k_Walkable)
	{
		return;
	}
	cells[seed] = k_Queued;
	std::vector<glm::ivec2> waiting {seed};
	while (!waiting.empty())
	{
		const auto from = waiting.front();
		for (const auto& step : k_Neighbours)
		{
			const auto next = from + step;
			if (!cells.Contains(next))
			{
				continue;
			}
			auto& value = cells[next];
			if (value == k_Avoid)
			{
				value = k_AvoidNextToLand;
			}
			else if (value == k_Walkable)
			{
				value = k_Queued;
				waiting.push_back(next);
			}
		}
		cells[from] = k_Land;
		// The last waiting takes the place of the one walked from
		waiting.front() = waiting.back();
		waiting.pop_back();
	}
	for (auto& value : cells.values)
	{
		if (value == k_Avoid || value == k_Walkable)
		{
			value = k_Unreachable;
		}
		else if (value == k_AvoidNextToLand)
		{
			value = k_Avoid;
		}
	}
}
} // namespace

Map::Map(int32_t cellsPerSide, std::vector<uint8_t> cells)
    : _cellsPerSide(cellsPerSide)
    , _cells(std::move(cells))
{
}

uint8_t Map::At(glm::ivec2 cell) const
{
	if (cell.x < 0 || cell.y < 0 || cell.x >= _cellsPerSide || cell.y >= _cellsPerSide)
	{
		return k_Unreachable;
	}
	return _cells[(static_cast<size_t>(cell.y) * static_cast<size_t>(_cellsPerSide)) + static_cast<size_t>(cell.x)];
}

Map land_avoid::Build(int32_t cellsPerSide, const Altitude& altitude, const Water& water)
{
	std::vector<uint8_t> values(static_cast<size_t>(cellsPerSide) * static_cast<size_t>(cellsPerSide), k_Unreachable);
	const Cells cells {.size = cellsPerSide, .values = values};
	const auto cornerAltitude = [&](glm::ivec2 corner) -> uint8_t {
		return cells.Contains(corner) ? altitude(corner) : uint8_t {0};
	};

	glm::ivec2 seed {0};
	for (int32_t z = 0; z < cellsPerSide; ++z)
	{
		// A run of walkable cells along the row; the walk starts from the end of the last run a blocked cell ends
		bool inRun = false;
		int32_t runLength = 0;
		for (int32_t x = 0; x < cellsPerSide; ++x)
		{
			const std::array<uint8_t, 4> corners {cornerAltitude({x, z}), cornerAltitude({x, z + 1}),
			                                      cornerAltitude({x + 1, z}), cornerAltitude({x + 1, z + 1})};
			float highest = static_cast<float>(corners[0]) * k_HeightPerAltitude;
			float lowest = highest;
			for (size_t i = 1; i < corners.size(); ++i)
			{
				const float height = static_cast<float>(corners.at(i)) * k_HeightPerAltitude;
				highest = std::max(highest, height);
				lowest = std::min(lowest, height);
			}
			const bool underSea = std::ranges::all_of(corners, [](uint8_t corner) { return corner == 0; });
			if (highest - lowest > k_MaxRise || underSea)
			{
				cells[{x, z}] = k_Avoid;
				if (inRun && runLength > 0)
				{
					seed = {x - 1, z};
				}
				inRun = false;
				runLength = 0;
				continue;
			}
			cells[{x, z}] = k_Walkable;
			inRun = true;
			++runLength;
		}
	}

	Walk(cells, seed);

	// Reached land that holds water is water
	for (int32_t z = 0; z < cellsPerSide; ++z)
	{
		for (int32_t x = 0; x < cellsPerSide; ++x)
		{
			if (cells[{x, z}] == k_Land && water({x, z}))
			{
				cells[{x, z}] = k_Water;
			}
		}
	}

	Map map(cellsPerSide, std::move(values));
	map.SetSeed(seed);
	return map;
}
