/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <LNDFile.h>
#include <gtest/gtest.h>

#include "3D/LandBlock.h"

using openblack::MakeLandVertex;

TEST(LandVertex, LandAtSeaLevelIsDrawnFlatButKeepsItsAltitude)
{
	// Altitudes 0 to 3 lie flat at sea level; the vertex still says how high the land is, so the small bump detail and
	// the shadows show on the flattened beach of altitudes 2 and 3
	for (uint8_t altitude = 0; altitude <= 3; ++altitude)
	{
		const auto vertex = MakeLandVertex({20.0f, 30.0f}, altitude);
		EXPECT_FLOAT_EQ(vertex.position.x, 20.0f);
		EXPECT_FLOAT_EQ(vertex.position.y, 0.0f);
		EXPECT_FLOAT_EQ(vertex.position.z, 30.0f);
		EXPECT_FLOAT_EQ(vertex.altitude, static_cast<float>(altitude));
	}
}

TEST(LandVertex, HigherLandRisesByItsAltitude)
{
	for (const uint8_t altitude : {uint8_t {4}, uint8_t {100}, uint8_t {255}})
	{
		const auto vertex = MakeLandVertex({0.0f, 160.0f}, altitude);
		EXPECT_FLOAT_EQ(vertex.position.y, static_cast<float>(altitude) * 0.67f);
		EXPECT_FLOAT_EQ(vertex.position.z, 160.0f);
		EXPECT_FLOAT_EQ(vertex.altitude, static_cast<float>(altitude));
	}
}

TEST(LandBlock, ItsHighestAltitudeIsTheHighestCellWhenItWasLoaded)
{
	openblack::lnd::LNDBlock loaded {};
	loaded.cells.at(5).altitude = 40;
	loaded.cells.at(200).altitude = 90;
	openblack::LandBlock block;
	block.SetLndBlock(loaded);
	EXPECT_EQ(block.GetHighestAltitude(), 90);
	// A cell raised later does not change it, and the view test of the block keeps the box of the land as it was
	block.SetCellAltitude(7, 200);
	EXPECT_EQ(block.GetHighestAltitude(), 90);
}

TEST(LandBlock, ACellThatAlreadyHasTheAltitudeDoesNotChange)
{
	openblack::lnd::LNDBlock loaded {};
	loaded.cells.at(7).altitude = 40;
	openblack::LandBlock block;
	block.SetLndBlock(loaded);
	EXPECT_FALSE(block.SetCellAltitude(7, 40));
	EXPECT_TRUE(block.SetCellAltitude(7, 41));
	EXPECT_EQ(block.GetLndBlock()->cells.at(7).altitude, 41);
	EXPECT_FALSE(block.SetCellAltitude(7, 41));
	// A place past the block's cells changes nothing
	EXPECT_FALSE(block.SetCellAltitude(loaded.cells.size(), 9));
}
