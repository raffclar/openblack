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
#include <span>
#include <vector>

#include <glm/vec2.hpp>

/// The lakes above the sea of a generated land. The lands of the story have one water level, the sea's: their lakes are
/// land under the sea level, which shows the sea through it. A generated land can also have lakes on the hills, each
/// with its own level (deviation 233).
namespace openblack::land_lakes
{
/// The corners along each side of the map
inline constexpr int k_CornersPerSide = 513;
/// The cells of the map are 10 units a side
inline constexpr float k_CellSize = 10.0f;
/// The height of the sea's water, where the game draws the sea
inline constexpr float k_SeaLevel = 0.0f;

/// A lake: the height of its water, and the map corners that its water and its shore cover
struct Lake
{
	/// The height of the water, in the units of the world
	float level;
	/// The corners at x * 513 + z, in order
	std::vector<uint32_t> corners;
};

/// The lakes of a land and the lake of each corner, for quick questions about the water at a place
class LakeMap
{
public:
	LakeMap() = default;
	explicit LakeMap(std::vector<Lake> lakes);

	[[nodiscard]] bool Empty() const { return _lakes.empty(); }
	[[nodiscard]] std::span<const Lake> GetLakes() const { return _lakes; }
	/// A value that only the levels and the corners of the lakes give, to know when the lakes change
	[[nodiscard]] uint64_t Digest() const { return _digest; }

	/// The index of the lake whose water or shore covers the corner. None off the map or where no lake is.
	[[nodiscard]] std::optional<size_t> LakeOfCorner(int x, int z) const;
	/// The lake of the cell of a place: the lake of a corner of the cell. None where no corner of the cell is in a lake.
	[[nodiscard]] std::optional<size_t> LakeAt(glm::vec2 position) const;
	/// The height of the water at a place: the level of the lake of the cell, or else the sea level. The ground is under
	/// the water where it is lower than this.
	[[nodiscard]] float WaterLevelAt(glm::vec2 position) const;

private:
	std::vector<Lake> _lakes;
	/// One more than the index of the lake of each corner, or 0
	std::vector<uint16_t> _cornerLakes;
	uint64_t _digest {0};
};
} // namespace openblack::land_lakes
