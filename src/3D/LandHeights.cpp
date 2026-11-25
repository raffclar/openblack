/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandHeights.h"

#include <LNDFile.h>

#include "3D/LandIslandInterface.h"

using namespace openblack;

namespace
{
/// The 1/256 of the game's integer interpolation of the heights
constexpr float k_AltitudeFraction = 1.0f / 256.0f;
} // namespace

float land_heights::AltitudeInCell(uint32_t fractionX, uint32_t fractionZ, const CellCorners& corners, bool seaLevelFlat)
{
	const auto clamp = seaLevelFlat && corners.u00 <= k_SeaLevelClampAltitude;
	const auto altitude = [clamp](int32_t units) -> int32_t { return clamp && units <= k_SeaLevelAltitude ? 0 : units; };
	const auto a00 = altitude(corners.u00);
	const auto a01 = altitude(corners.u01);
	const auto a10 = altitude(corners.u10);
	const auto a11 = altitude(corners.u11);

	// Complete the plane of the triangle the point is in. Cells split from x + 1 to z + 1 rather than corner to corner.
	int32_t v00 = a00;
	int32_t v01 = a01;
	int32_t v10 = a10;
	int32_t v11 = a11;
	if (corners.split)
	{
		if (fractionZ > 0xFFFF - fractionX)
		{
			v00 = a10 - a11 + a01;
		}
		else
		{
			v11 = a10 - a00 + a01;
		}
	}
	else if (fractionX > fractionZ)
	{
		v01 = a00 - a10 + a11;
	}
	else
	{
		v10 = a00 - a01 + a11;
	}

	const auto z8 = static_cast<int32_t>(fractionZ >> 8);
	const auto x8 = static_cast<int32_t>(fractionX >> 8);
	const auto edgeX0 = ((v01 - v00) * z8) + (v00 << 8);
	const auto edgeX1 = ((v11 - v10) * z8) + (v10 << 8);
	const auto height = (((edgeX1 - edgeX0) * x8) >> 8) + edgeX0;
	return static_cast<float>(height) * LandIslandInterface::k_HeightUnit * k_AltitudeFraction;
}

void LandHeights::Follow(const LandIslandInterface& island)
{
	const auto changes = island.HeightChanges();
	const auto digest = island.LandDigest();
	if (_copies != 0 && _source == &island && _sourceChanges == changes && _sourceDigest == digest)
	{
		return;
	}
	_source = &island;
	_sourceChanges = changes;
	_sourceDigest = digest;
	++_copies;
	_blockAt.fill(0);
	_blocks.clear();
	if (!island.IsLoaded())
	{
		return;
	}
	// A block keeps its own copy of the first row and column of the next blocks. The land reads the four corners of a
	// cell from the block of the cell, thus the copy keeps the 17 by 17 cells of each block.
	constexpr auto k_CellsPerBlock = static_cast<int32_t>(k_BlockSide - 1);
	for (size_t blockX = 0; blockX < k_BlocksPerSide; ++blockX)
	{
		for (size_t blockZ = 0; blockZ < k_BlocksPerSide; ++blockZ)
		{
			const auto firstX = static_cast<int32_t>(blockX) * k_CellsPerBlock;
			const auto firstZ = static_cast<int32_t>(blockZ) * k_CellsPerBlock;
			const auto* cells = island.FindCell({static_cast<uint16_t>(firstX), static_cast<uint16_t>(firstZ)});
			if (cells == nullptr)
			{
				continue;
			}
			auto& block = _blocks.emplace_back();
			for (size_t x = 0; x < k_BlockSide; ++x)
			{
				for (size_t z = 0; z < k_BlockSide; ++z)
				{
					const auto index = (x * k_BlockSide) + z;
					const auto& cell = cells[index];
					block.units.at(index) =
					    island.HeightUnitsOf(cell, firstX + static_cast<int32_t>(x), firstZ + static_cast<int32_t>(z));
					block.split.set(index, cell.properties.split != 0);
				}
			}
			_blockAt.at((blockX * k_BlocksPerSide) + blockZ) = static_cast<uint16_t>(_blocks.size());
		}
	}
}

void LandHeights::Clear()
{
	_blockAt.fill(0);
	_blocks.clear();
	_source = nullptr;
	_copies = 0;
}

const LandHeights::Block* LandHeights::BlockOf(int32_t cellX, int32_t cellZ) const
{
	if (cellX < 0 || cellZ < 0 || cellX >= LandIslandInterface::k_MapCellsPerSide ||
	    cellZ >= LandIslandInterface::k_MapCellsPerSide)
	{
		return nullptr;
	}
	const auto at = _blockAt.at((static_cast<size_t>(cellX >> 4) * k_BlocksPerSide) + static_cast<size_t>(cellZ >> 4));
	return at == 0 ? nullptr : &_blocks.at(at - 1u);
}

float LandHeights::HeightAt(glm::vec2 position) const
{
	return Altitude(LandIslandInterface::ToMapCoords(position.x), LandIslandInterface::ToMapCoords(position.y));
}

float LandHeights::Altitude(int32_t mapX, int32_t mapZ, bool seaLevelFlat) const
{
	const auto cellX = static_cast<int16_t>(static_cast<uint32_t>(mapX) >> 16);
	const auto cellZ = static_cast<int16_t>(static_cast<uint32_t>(mapZ) >> 16);
	const auto* block = BlockOf(cellX, cellZ);
	if (block == nullptr)
	{
		return 0.0f;
	}
	const auto index = (static_cast<size_t>(cellX & 0xF) * k_BlockSide) + static_cast<size_t>(cellZ & 0xF);
	const land_heights::CellCorners corners {
	    .u00 = block->units.at(index),
	    .u01 = block->units.at(index + 1),
	    .u10 = block->units.at(index + k_BlockSide),
	    .u11 = block->units.at(index + k_BlockSide + 1),
	    .split = block->split.test(index),
	};
	return land_heights::AltitudeInCell(static_cast<uint32_t>(mapX) & 0xFFFF, static_cast<uint32_t>(mapZ) & 0xFFFF, corners,
	                                    seaLevelFlat);
}

uint16_t LandHeights::CornerUnits(glm::u16vec2 corner) const
{
	const auto* block = BlockOf(corner.x, corner.y);
	if (block == nullptr)
	{
		return 0;
	}
	return block->units.at((static_cast<size_t>(corner.x & 0xFu) * k_BlockSide) + static_cast<size_t>(corner.y & 0xFu));
}
