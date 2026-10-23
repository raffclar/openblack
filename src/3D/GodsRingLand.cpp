/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GodsRingLand.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

using namespace openblack;

namespace
{
constexpr int k_CellsPerBlock = 16;
constexpr float k_BlockSize = 160.0f;
/// A block has the corners of its 16 cells and the corners along its far sides
constexpr int k_BlockCornersPerSide = k_CellsPerBlock + 1;

/// How many cells the corner (x, z) is in from the sea bed; 0 on the sea bed
int CellsFromSea(int x, int z)
{
	const glm::vec2 corner {static_cast<float>(x) * gods_ring_land::k_CellSize,
	                        static_cast<float>(z) * gods_ring_land::k_CellSize};
	const float inside = gods_ring_land::k_SeaRadius - glm::distance(corner, gods_ring_land::k_MapMiddle);
	return std::max(0, static_cast<int>(std::ceil(inside / gods_ring_land::k_CellSize)));
}

/// Whether some cell of the block (x, z) is not open sea
bool BlockHasLand(int blockX, int blockZ)
{
	for (int cx = 0; cx < k_CellsPerBlock; ++cx)
	{
		for (int cz = 0; cz < k_CellsPerBlock; ++cz)
		{
			const auto kind = gods_ring_land::KindOf((blockX * k_CellsPerBlock) + cx, (blockZ * k_CellsPerBlock) + cz);
			if (kind != flat_land::CellKind::OpenWater)
			{
				return true;
			}
		}
	}
	return false;
}
} // namespace

uint8_t gods_ring_land::Altitude(int x, int z)
{
	const auto in = CellsFromSea(x, z);
	return in < static_cast<int>(flat_land::k_ShoreAltitudes.size()) ? flat_land::k_ShoreAltitudes.at(static_cast<size_t>(in))
	                                                                 : flat_land::k_Altitude;
}

flat_land::CellKind gods_ring_land::KindOf(int x, int z)
{
	const std::array corners {Altitude(x, z), Altitude(x + 1, z), Altitude(x, z + 1), Altitude(x + 1, z + 1)};
	const auto [low, high] = std::ranges::minmax(corners);
	if (high == 0)
	{
		return flat_land::CellKind::OpenWater;
	}
	if (high <= flat_land::k_SeaLevelAltitude)
	{
		return flat_land::CellKind::Shallows;
	}
	return low <= flat_land::k_SeaLevelAltitude ? flat_land::CellKind::Shore : flat_land::CellKind::Land;
}

lnd::LNDCell gods_ring_land::CellAt(int x, int z)
{
	lnd::LNDCell cell {};
	cell.luminosity = flat_land::k_Luminosity;
	cell.altitude = Altitude(x, z);
	switch (KindOf(x, z))
	{
	case flat_land::CellKind::OpenWater:
		cell.properties.hasWater = 1;
		cell.flags = k_SeaSoundFlags | flat_land::k_OpenWaterFlag;
		break;
	case flat_land::CellKind::Shallows:
		cell.properties.hasWater = 1;
		cell.flags = flat_land::k_CoastalSoundFlags;
		break;
	case flat_land::CellKind::Shore:
		// The bank is the coast: the game puts nothing on it
		cell.properties.coastLine = 1;
		cell.flags = flat_land::k_CoastalSoundFlags;
		break;
	case flat_land::CellKind::Land:
		cell.flags = flat_land::k_SoundFlags;
		break;
	}
	return cell;
}

LandData gods_ring_land::Build()
{
	LandData data;
	for (int x = 0; x < LandData::k_BlocksPerSide; ++x)
	{
		for (int z = 0; z < LandData::k_BlocksPerSide; ++z)
		{
			if (!BlockHasLand(x, z))
			{
				continue;
			}
			auto& block = data.blocks.emplace_back();
			for (int cx = 0; cx < k_BlockCornersPerSide; ++cx)
			{
				for (int cz = 0; cz < k_BlockCornersPerSide; ++cz)
				{
					const auto mapX = (x * k_CellsPerBlock) + cx;
					const auto mapZ = (z * k_CellsPerBlock) + cz;
					block.cells.at(static_cast<size_t>((cx * k_BlockCornersPerSide) + cz)) = CellAt(mapX, mapZ);
				}
			}
			block.index = static_cast<uint32_t>(data.blocks.size());
			block.mapX = static_cast<float>(x) * k_BlockSize;
			block.mapZ = static_cast<float>(z) * k_BlockSize;
			block.blockX = static_cast<uint32_t>(x);
			block.blockZ = static_cast<uint32_t>(z);
			data.blockIndexLookup.at(static_cast<size_t>(x * LandData::k_BlocksPerSide + z)) =
			    static_cast<uint16_t>(block.index);
		}
	}

	// One country, the grass material at every height
	auto& country = data.countries.emplace_back();
	std::ranges::fill(country.materials, lnd::LNDMapMaterial {.indices = {0, 0}, .coefficient = 0});
	data.materials.push_back(flat_land::CheckerMaterial());

	data.noise.assign(static_cast<size_t>(lnd::LNDBumpMap::k_Width) * lnd::LNDBumpMap::k_Height, 0);
	data.bump.assign(static_cast<size_t>(lnd::LNDBumpMap::k_Width) * lnd::LNDBumpMap::k_Height, flat_land::k_FlatBump);
	return data;
}
