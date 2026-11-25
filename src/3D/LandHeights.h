/*******************************************************************************
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

#include <array>
#include <bitset>
#include <vector>

#include <glm/gtc/type_precision.hpp>
#include <glm/vec2.hpp>

namespace openblack
{
class LandIslandInterface;

namespace land_heights
{
/// Land at or below this altitude is at sea level. The game draws it at height 0.
constexpr uint8_t k_SeaLevelAltitude = 3;
/// In a cell whose first corner is no higher than this, the corners at sea level count as height 0
constexpr uint8_t k_SeaLevelClampAltitude = 4;

/// The heights of the four corners of a cell and how the cell is cut into two triangles
struct CellCorners
{
	/// The heights in units of the corners: x and z, x and z + 1, x + 1 and z, x + 1 and z + 1
	int32_t u00 {0};
	int32_t u01 {0};
	int32_t u10 {0};
	int32_t u11 {0};
	/// True when the cell is cut from the corner x + 1 to the corner z + 1, not from corner to corner
	bool split {false};
};

/// The height of the land at a point in a cell, with the integer steps of the game. The fractions are the place of the
/// point in the cell, in 1/65536 of a cell. The point is in one of the two triangles of the cell, and the height is on
/// the plane of that triangle. Land at sea level is flat unless `seaLevelFlat` is false.
[[nodiscard]] float AltitudeInCell(uint32_t fractionX, uint32_t fractionZ, const CellCorners& corners, bool seaLevelFlat);
} // namespace land_heights

/// A copy of the heights of the land, which gives the same heights as the land. The steps of the frame update it on the
/// game's thread, thus the draw reads heights that no turn changes while it draws. The copy is made again only when the
/// heights of the land changed.
class LandHeights
{
public:
	/// Makes the copy again when the heights of the land changed since the last copy, or when the land is another land
	void Follow(const LandIslandInterface& island);
	/// Removes the copy: each height is 0
	void Clear();

	/// The height of the land at a world position, as LandIslandInterface::GetHeightAt gives it
	[[nodiscard]] float HeightAt(glm::vec2 position) const;
	/// The height of the land at a map position in 1/65536 of a cell, as LandIslandInterface::GetAltitude gives it
	[[nodiscard]] float Altitude(int32_t mapX, int32_t mapZ, bool seaLevelFlat = true) const;
	/// The height of a map corner in units, as LandIslandInterface::CornerUnits gives it
	[[nodiscard]] uint16_t CornerUnits(glm::u16vec2 corner) const;
	/// How many times the copy was made
	[[nodiscard]] uint64_t Copies() const { return _copies; }

private:
	/// The cells along each side of a block, with the first row and column of the next blocks
	static constexpr size_t k_BlockSide = 17;
	/// The blocks along each side of the map
	static constexpr size_t k_BlocksPerSide = 32;

	/// The heights in units and the cuts of the cells of a block, [x * 17 + z]
	struct Block
	{
		std::array<uint16_t, k_BlockSide * k_BlockSide> units {};
		std::bitset<k_BlockSide * k_BlockSide> split;
	};

	/// The block of a cell, or null when the cell is off the map or has no block
	[[nodiscard]] const Block* BlockOf(int32_t cellX, int32_t cellZ) const;

	/// For each place of a block on the map, the index of its block plus 1, or 0 where there is no block
	std::array<uint16_t, k_BlocksPerSide * k_BlocksPerSide> _blockAt {};
	std::vector<Block> _blocks;
	/// What the copy was made from
	const LandIslandInterface* _source {nullptr};
	uint64_t _sourceChanges {0};
	uint64_t _sourceDigest {0};
	uint64_t _copies {0};
};
} // namespace openblack
