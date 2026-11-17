/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandLakes.h"

#include <cmath>

#include <array>
#include <bit>

using namespace openblack::land_lakes;

namespace
{
constexpr size_t k_CornerCount = static_cast<size_t>(k_CornersPerSide) * static_cast<size_t>(k_CornersPerSide);
}

LakeMap::LakeMap(std::vector<Lake> lakes)
    : _lakes(std::move(lakes))
{
	if (_lakes.empty())
	{
		return;
	}
	_cornerLakes.assign(k_CornerCount, 0);
	// FNV-1a over the levels and the corners
	_digest = 0xCBF29CE484222325ull;
	const auto mix = [this](uint64_t value) {
		_digest ^= value;
		_digest *= 0x100000001B3ull;
	};
	for (size_t i = 0; i < _lakes.size(); ++i)
	{
		mix(std::bit_cast<uint32_t>(_lakes[i].level));
		for (const auto corner : _lakes[i].corners)
		{
			mix(corner);
			if (corner < k_CornerCount)
			{
				_cornerLakes[corner] = static_cast<uint16_t>(i + 1);
			}
		}
	}
}

std::optional<size_t> LakeMap::LakeOfCorner(int x, int z) const
{
	if (_cornerLakes.empty() || x < 0 || z < 0 || x >= k_CornersPerSide || z >= k_CornersPerSide)
	{
		return std::nullopt;
	}
	const auto lake = _cornerLakes[(static_cast<size_t>(x) * k_CornersPerSide) + static_cast<size_t>(z)];
	return lake != 0 ? std::optional<size_t>(lake - 1u) : std::nullopt;
}

std::optional<size_t> LakeMap::LakeAt(glm::vec2 position) const
{
	if (_cornerLakes.empty())
	{
		return std::nullopt;
	}
	const int x = static_cast<int>(std::floor(position.x / k_CellSize));
	const int z = static_cast<int>(std::floor(position.y / k_CellSize));
	constexpr std::array<std::array<int, 2>, 4> k_Corners {{{0, 0}, {1, 0}, {0, 1}, {1, 1}}};
	for (const auto& [dx, dz] : k_Corners)
	{
		if (const auto lake = LakeOfCorner(x + dx, z + dz))
		{
			return lake;
		}
	}
	return std::nullopt;
}

float LakeMap::WaterLevelAt(glm::vec2 position) const
{
	const auto lake = LakeAt(position);
	return lake.has_value() ? _lakes[*lake].level : k_SeaLevel;
}
