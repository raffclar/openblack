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

#include <functional>
#include <vector>

#include <glm/vec2.hpp>

/// Where a creature may go on a land: one value for each of the land's cells, worked out once when the land is loaded.
/// A cell is walkable when the four corners of its square differ in height by at most 10 and are not all at sea level.
/// Of the walkable cells, only those a creature can reach by walking from one place on the land count: the last
/// walkable cell before a blocked one, scanning the rows. Reached cells that hold water are marked as water; blocked
/// cells next to reached ones as to be avoided; everything else as out of reach.
namespace openblack::land_avoid
{

/// What a cell is, as the map holds it
constexpr uint8_t k_Land = 0;        ///< reached, walkable land
constexpr uint8_t k_Avoid = 1;       ///< too steep, or under the sea, next to reached land
constexpr uint8_t k_Unreachable = 2; ///< not reached, or too steep or under the sea away from reached land
constexpr uint8_t k_Water = 6;       ///< reached, walkable, but water

/// A cell is too steep when its corners' heights differ by more than this
constexpr float k_MaxRise = 10.0f;
/// A corner's height for each step of its altitude
constexpr float k_HeightPerAltitude = 0.67f;

class Map
{
public:
	Map() = default;
	Map(int32_t cellsPerSide, std::vector<uint8_t> cells);

	/// The cell's value; out of reach off the map
	[[nodiscard]] uint8_t At(glm::ivec2 cell) const;
	[[nodiscard]] int32_t CellsPerSide() const { return _cellsPerSide; }
	/// The cell the walk was worked out from
	[[nodiscard]] glm::ivec2 Seed() const { return _seed; }
	void SetSeed(glm::ivec2 seed) { _seed = seed; }

private:
	int32_t _cellsPerSide {0};
	/// By row of z, then x
	std::vector<uint8_t> _cells;
	glm::ivec2 _seed {0};
};

/// The altitude of a corner of the cells (0 off the map), and whether a cell holds water (or has no land)
using Altitude = std::function<uint8_t(glm::ivec2 corner)>;
using Water = std::function<bool(glm::ivec2 cell)>;

/// Works out the map of a land of this many cells a side
[[nodiscard]] Map Build(int32_t cellsPerSide, const Altitude& altitude, const Water& water);

} // namespace openblack::land_avoid
