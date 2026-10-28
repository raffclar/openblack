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
#include <cstdint>

#include <bitset>
#include <functional>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

/// The map that creatures use to find hills, coasts, the sea, towns and land to go to. The land divides into 64 x 64
/// regions of 8 x 8 cells. When the land is ready, each region gets the kinds of land that it has. Each creature keeps
/// the regions that it went into, so that it can prefer a region that it did not see yet.
namespace openblack::creature_exploration
{

/// Regions along each side of the map
constexpr int32_t k_RegionsPerSide = 64;
/// Cells along each side of a region
constexpr int32_t k_CellsPerRegion = 8;
/// All the regions of the map
constexpr size_t k_RegionCount = static_cast<size_t>(k_RegionsPerSide) * k_RegionsPerSide;
/// A search looks at this many regions, in a spiral out from its start
constexpr uint32_t k_SpiralSteps = 4096;
/// A hill is higher than this part of the highest region of the land
constexpr float k_HillPartOfHighest = 0.5f;
/// The length of a side of a cell, in metres
constexpr float k_CellSize = 10.0f;

/// The kinds of land that a region can have
enum class Kind : uint8_t
{
	/// A cell of the region has a building of a town
	Town,
	/// A cell of the region is dry land at the edge of the water
	Coast,
	/// A cell of the region is water, or is not on a land block
	Sea,
	/// The region is high on the land and no region next to it is higher
	Hill,
	/// A cell of the region is dry land
	Land,
};

/// What the map knows of one cell when it makes the regions
struct CellLand
{
	/// The height of the cell, in metres
	float height {0.0f};
	bool water {false};
	bool coast {false};
};

/// The cell at a place, none when the place is not on a land block
using CellAt = std::function<std::optional<CellLand>(glm::ivec2 cell)>;
/// Whether a building of a town stands in a cell
using TownAt = std::function<bool(glm::ivec2 cell)>;

/// One region of the map
struct Region
{
	/// The height of the highest cell, and that cell. Of cells of equal height, the last cell in the order x then z
	/// counts.
	float highest {0.0f};
	glm::ivec2 highestCell {0};
	/// The first coast cell and the first sea cell in the order x then z, none when there is no such cell
	std::optional<glm::ivec2> firstCoast;
	std::optional<glm::ivec2> firstSea;
	/// The kinds of land that the region has, a bit for each kind
	uint8_t kinds {0};

	[[nodiscard]] bool Is(Kind kind) const;
};

/// The regions that a creature went into
using Visited = std::bitset<k_RegionCount>;

/// The regions of the land. The world keeps the map: a computer that takes the world of another computer gets the same
/// map, made from the towns at the start of the land.
struct RegionMap
{
	/// Makes the map from the cells of the land and the buildings of the towns that stand on it now
	[[nodiscard]] static RegionMap Make(const CellAt& cellAt, const TownAt& townAt);

	[[nodiscard]] const Region& At(glm::ivec2 region) const;
	/// The height of the highest region of the land
	[[nodiscard]] float Highest() const { return highest; }

	/// The regions, by their index
	std::vector<Region> regions;
	float highest {0.0f};
};

/// Whether a region is on the map
[[nodiscard]] bool OnMap(glm::ivec2 region);
/// The region of a place on the land, in metres
[[nodiscard]] glm::ivec2 RegionOf(glm::vec2 place);
/// Notes that a creature went into a region
void Visit(Visited& visited, glm::ivec2 region);
[[nodiscard]] bool WasVisited(const Visited& visited, glm::ivec2 region);

/// A search for the nearest region of a kind
struct Search
{
	Kind kind {Kind::Land};
	/// The region where the search starts
	glm::ivec2 from {0};
	/// The search continues past regions that the creature went into, and gives the first region that it did not. When
	/// the creature went into all of them, the search gives the last region that it found.
	bool preferUnvisited {false};
	/// The region where the search starts can be the result
	bool includeStart {true};
};

/// The nearest region of the kind, in a spiral out from the start. None when the spiral finds no such region.
[[nodiscard]] std::optional<glm::ivec2> FindNearest(const RegionMap& map, const Visited& visited, const Search& search);

/// The cell of a region that a creature goes to for a kind of land:
/// - a town: the first cell in the order x then z with a building of a town, else the first cell of the region;
/// - a coast or the sea: the first cell of that kind, else the first cell of the region;
/// - a hill: the highest cell;
/// - land: the cell at the middle of the region.
[[nodiscard]] glm::ivec2 CellFor(const RegionMap& map, glm::ivec2 region, Kind kind, const TownAt& townAt);

/// The place of the low corner of a cell on the land, in metres
[[nodiscard]] glm::vec2 PlaceOf(glm::ivec2 cell);

/// Up to this stage of growing up, the creature can want to explore at all times
constexpr uint32_t k_WanderAlwaysToStage = 5;
/// After that stage, the creature wants to explore only while its exhaustion is below this
constexpr float k_TooExhaustedToWander = 0.8f;
/// Whether the creature can plan for its desire to explore (wanderlust): when it is young, or not too exhausted
[[nodiscard]] bool WanderlustAllowed(uint32_t stage, float exhaustion);

} // namespace openblack::creature_exploration
