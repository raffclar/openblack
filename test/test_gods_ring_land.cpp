/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

#include "3D/GodsRingLand.h"

using namespace openblack;
using namespace openblack::gods_ring_land;

namespace
{
constexpr int k_Middle = k_CellsPerSide / 2;
} // namespace

TEST(GodsRingLand, TheMiddleIsFlatLandAndTheCornersAreSea)
{
	EXPECT_EQ(KindOf(k_Middle, k_Middle), flat_land::CellKind::Land);
	EXPECT_EQ(Altitude(k_Middle, k_Middle), flat_land::k_Altitude);
	EXPECT_EQ(KindOf(0, 0), flat_land::CellKind::OpenWater);
	EXPECT_EQ(KindOf(k_CellsPerSide - 1, k_CellsPerSide - 1), flat_land::CellKind::OpenWater);
	EXPECT_EQ(Altitude(0, k_Middle), 0);
}

TEST(GodsRingLand, TheShoreGoesDownFromThePlaneToTheSea)
{
	// Along the line east from the middle, the land is the plane, then the shore, then the sea
	const int planeEnd = k_Middle + static_cast<int>(k_PlaneRadius / k_CellSize);
	EXPECT_EQ(Altitude(planeEnd, k_Middle), flat_land::k_Altitude);
	uint8_t last = flat_land::k_Altitude;
	for (int x = planeEnd; x < k_CellsPerSide; ++x)
	{
		const auto altitude = Altitude(x, k_Middle);
		EXPECT_LE(altitude, last) << x;
		last = altitude;
	}
	EXPECT_EQ(last, 0);
	EXPECT_EQ(Altitude(k_Middle + static_cast<int>(k_SeaRadius / k_CellSize) + 1, k_Middle), 0);
}

TEST(GodsRingLand, TheIslandIsTheSameOnEachQuarterOfTheMap)
{
	for (int x = 0; x <= k_CellsPerSide; x += 3)
	{
		for (int z = 0; z <= k_CellsPerSide; z += 3)
		{
			const auto altitude = Altitude(x, z);
			EXPECT_EQ(Altitude(k_CellsPerSide - x, z), altitude) << x << " " << z;
			EXPECT_EQ(Altitude(x, k_CellsPerSide - z), altitude) << x << " " << z;
			EXPECT_EQ(Altitude(z, x), altitude) << x << " " << z;
		}
	}
}

TEST(GodsRingLand, TheCellsOfWaterHoldWaterAndTheSeaIsOpen)
{
	const auto sea = CellAt(0, 0);
	EXPECT_EQ(sea.properties.hasWater, 1);
	EXPECT_EQ(sea.flags, k_SeaSoundFlags | flat_land::k_OpenWaterFlag);
	const auto land = CellAt(k_Middle, k_Middle);
	EXPECT_EQ(land.properties.hasWater, 0);
	EXPECT_EQ(land.flags, flat_land::k_SoundFlags);
}

TEST(GodsRingLand, ABlockOfOnlySeaHasNoLand)
{
	const auto data = Build();
	EXPECT_LT(data.blocks.size(), data.blockIndexLookup.size());
	EXPECT_GT(data.blocks.size(), 0u);
	// The corner block is sea; the middle block is land
	EXPECT_EQ(data.blockIndexLookup.at(0), 0);
	constexpr int k_MiddleBlock = LandData::k_BlocksPerSide / 2;
	EXPECT_NE(data.blockIndexLookup.at(static_cast<size_t>((k_MiddleBlock * LandData::k_BlocksPerSide) + k_MiddleBlock)), 0);
	for (size_t i = 0; i < data.blocks.size(); ++i)
	{
		const auto& block = data.blocks.at(i);
		EXPECT_EQ(block.index, i + 1);
		EXPECT_EQ(data.blockIndexLookup.at((block.blockX * LandData::k_BlocksPerSide) + block.blockZ), i + 1);
	}
	ASSERT_EQ(data.materials.size(), 1u);
	EXPECT_EQ(data.materials.front().type, flat_land::k_GrassMaterial);
	EXPECT_EQ(data.countries.size(), 1u);
}

TEST(GodsRingLand, TheBuildHasTheCellsOfCellAtAndEachBlockWithLand)
{
	const auto data = Build();
	constexpr int k_Cells = 16;
	constexpr int k_Corners = k_Cells + 1;
	size_t withLand = 0;
	for (int x = 0; x < LandData::k_BlocksPerSide; ++x)
	{
		for (int z = 0; z < LandData::k_BlocksPerSide; ++z)
		{
			bool land = false;
			for (int cx = 0; cx < k_Cells && !land; ++cx)
			{
				for (int cz = 0; cz < k_Cells && !land; ++cz)
				{
					land = KindOf((x * k_Cells) + cx, (z * k_Cells) + cz) != flat_land::CellKind::OpenWater;
				}
			}
			const auto index = data.blockIndexLookup.at(static_cast<size_t>((x * LandData::k_BlocksPerSide) + z));
			EXPECT_EQ(index != 0, land) << "block " << x << " " << z;
			if (index == 0)
			{
				continue;
			}
			++withLand;
			const auto& block = data.blocks.at(index - 1u);
			for (int cx = 0; cx < k_Corners; ++cx)
			{
				for (int cz = 0; cz < k_Corners; ++cz)
				{
					const auto expected = CellAt((x * k_Cells) + cx, (z * k_Cells) + cz);
					const auto& cell = block.cells.at(static_cast<size_t>((cx * k_Corners) + cz));
					ASSERT_EQ(std::memcmp(&cell, &expected, sizeof(cell)), 0) << "block " << x << " " << z;
				}
			}
		}
	}
	EXPECT_EQ(withLand, data.blocks.size());
}
