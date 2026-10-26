/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureExplorationMap.h"

#include <cmath>

#include <algorithm>
#include <array>

using namespace openblack;
using namespace openblack::creature_exploration;

namespace
{
/// The four steps of the spiral and of the test for a hill: east, north, west and south
constexpr std::array<glm::ivec2, 4> k_Directions {glm::ivec2 {1, 0}, glm::ivec2 {0, 1}, glm::ivec2 {-1, 0}, glm::ivec2 {0, -1}};

uint8_t Bit(Kind kind)
{
	return static_cast<uint8_t>(1U << static_cast<uint8_t>(kind));
}

size_t IndexOf(glm::ivec2 region)
{
	return static_cast<size_t>(region.x) * k_RegionsPerSide + static_cast<size_t>(region.y);
}

glm::ivec2 FirstCell(glm::ivec2 region)
{
	return region * k_CellsPerRegion;
}

/// The cells of a region in the order x then z
template <typename Visit>
void EachCell(glm::ivec2 region, Visit&& visit)
{
	const auto first = FirstCell(region);
	for (int32_t x = first.x; x < first.x + k_CellsPerRegion; ++x)
	{
		for (int32_t z = first.y; z < first.y + k_CellsPerRegion; ++z)
		{
			if (!visit(glm::ivec2 {x, z}))
			{
				return;
			}
		}
	}
}

/// The spiral turns after one step, one step, two steps, two steps, three steps and so on
struct Spiral
{
	uint32_t direction {1};
	uint32_t left {1};

	glm::ivec2 Next()
	{
		if (--left == 0)
		{
			++direction;
			left = direction / 2;
		}
		return k_Directions.at(direction & 3U);
	}
};
} // namespace

bool Region::Is(Kind kind) const
{
	return (kinds & Bit(kind)) != 0;
}

RegionMap RegionMap::Make(const CellAt& cellAt, const TownAt& townAt)
{
	RegionMap map;
	map.regions.resize(k_RegionCount);
	// The height of each region first, as a hill needs the heights of all the regions
	for (int32_t rx = 0; rx < k_RegionsPerSide; ++rx)
	{
		for (int32_t rz = 0; rz < k_RegionsPerSide; ++rz)
		{
			auto& region = map.regions.at(IndexOf({rx, rz}));
			bool first = true;
			EachCell({rx, rz}, [&](glm::ivec2 cell) {
				const auto land = cellAt(cell);
				// A cell that is not on a land block has no height
				const float height = land.has_value() ? land->height : 0.0f;
				if (first || region.highest <= height)
				{
					first = false;
					region.highest = height;
					region.highestCell = cell;
				}
				return true;
			});
			map.highest = std::max(map.highest, region.highest);
		}
	}
	for (int32_t rx = 0; rx < k_RegionsPerSide; ++rx)
	{
		for (int32_t rz = 0; rz < k_RegionsPerSide; ++rz)
		{
			auto& region = map.regions.at(IndexOf({rx, rz}));
			bool town = false;
			bool land = false;
			EachCell({rx, rz}, [&](glm::ivec2 cell) {
				const auto here = cellAt(cell);
				const bool sea = !here.has_value() || here->water;
				if (sea && !region.firstSea.has_value())
				{
					region.firstSea = cell;
				}
				if (!sea && here->coast && !region.firstCoast.has_value())
				{
					region.firstCoast = cell;
				}
				land = land || !sea;
				town = town || townAt(cell);
				return true;
			});
			region.kinds = 0;
			if (town)
			{
				region.kinds |= Bit(Kind::Town);
			}
			if (region.firstCoast.has_value())
			{
				region.kinds |= Bit(Kind::Coast);
			}
			if (region.firstSea.has_value())
			{
				region.kinds |= Bit(Kind::Sea);
			}
			if (land)
			{
				region.kinds |= Bit(Kind::Land);
			}
			// A hill is higher than half the highest region, and no region next to it is higher
			bool hill = region.highest > map.highest * k_HillPartOfHighest;
			for (const auto direction : k_Directions)
			{
				const auto next = glm::ivec2 {rx, rz} + direction;
				if (hill && OnMap(next) && map.regions.at(IndexOf(next)).highest > region.highest)
				{
					hill = false;
				}
			}
			if (hill)
			{
				region.kinds |= Bit(Kind::Hill);
			}
		}
	}
	return map;
}

const Region& RegionMap::At(glm::ivec2 region) const
{
	return regions.at(IndexOf(region));
}

bool creature_exploration::OnMap(glm::ivec2 region)
{
	return region.x >= 0 && region.y >= 0 && region.x < k_RegionsPerSide && region.y < k_RegionsPerSide;
}

glm::ivec2 creature_exploration::RegionOf(glm::vec2 place)
{
	const auto cell = glm::ivec2 {static_cast<int32_t>(std::floor(place.x / k_CellSize)),
	                              static_cast<int32_t>(std::floor(place.y / k_CellSize))};
	return {cell.x >> 3, cell.y >> 3};
}

void creature_exploration::Visit(Visited& visited, glm::ivec2 region)
{
	if (OnMap(region))
	{
		visited.set(IndexOf(region));
	}
}

bool creature_exploration::WasVisited(const Visited& visited, glm::ivec2 region)
{
	return OnMap(region) && visited.test(IndexOf(region));
}

std::optional<glm::ivec2> creature_exploration::FindNearest(const RegionMap& map, const Visited& visited, const Search& search)
{
	std::optional<glm::ivec2> found;
	Spiral spiral;
	auto region = search.from;
	for (uint32_t step = 0; step < k_SpiralSteps; ++step)
	{
		if (OnMap(region) && map.At(region).Is(search.kind) && (search.includeStart || region != search.from))
		{
			found = region;
			if (!search.preferUnvisited || !WasVisited(visited, region))
			{
				return found;
			}
		}
		region += spiral.Next();
	}
	return found;
}

glm::ivec2 creature_exploration::CellFor(const RegionMap& map, glm::ivec2 region, Kind kind, const TownAt& townAt)
{
	const auto& here = map.At(region);
	switch (kind)
	{
	case Kind::Town:
	{
		std::optional<glm::ivec2> town;
		EachCell(region, [&](glm::ivec2 cell) {
			if (townAt(cell))
			{
				town = cell;
				return false;
			}
			return true;
		});
		return town.value_or(FirstCell(region));
	}
	case Kind::Coast:
		return here.firstCoast.value_or(FirstCell(region));
	case Kind::Sea:
		return here.firstSea.value_or(FirstCell(region));
	case Kind::Hill:
		return here.highestCell;
	case Kind::Land:
	default:
		return FirstCell(region) + k_CellsPerRegion / 2;
	}
}

glm::vec2 creature_exploration::PlaceOf(glm::ivec2 cell)
{
	return glm::vec2(cell) * k_CellSize;
}

bool creature_exploration::WanderlustAllowed(uint32_t stage, float exhaustion)
{
	return stage <= k_WanderAlwaysToStage || exhaustion < k_TooExhaustedToWander;
}
