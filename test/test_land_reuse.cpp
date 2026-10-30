/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <LNDFile.h>
#include <gtest/gtest.h>

#include "3D/LandReuse.h"

using namespace openblack;

namespace
{
/// A world state of the land of a file that is open now
land_reuse::Lands SameLandOfAFile()
{
	return {
	    .worldState = true,
	    .landLoaded = true,
	    .openLand = "Online Maps/mpm_2p_1.map",
	    .openLandscape = "Data/Landscape/MP/mpm_2p_1.lnd",
	    .savedLand = "Online Maps/mpm_2p_1.map",
	    .savedLandscape = "Data/Landscape/MP/mpm_2p_1.lnd",
	    .builtByGame = false,
	};
}

std::array<lnd::LNDCell, 17 * 17> Cells(uint8_t altitude)
{
	std::array<lnd::LNDCell, 17 * 17> cells {};
	for (auto& cell : cells)
	{
		cell.altitude = altitude;
	}
	return cells;
}
} // namespace

TEST(LandReuse, AWorldStateOfTheOpenLandKeepsTheLand)
{
	EXPECT_TRUE(land_reuse::KeepsLand(SameLandOfAFile()));
}

TEST(LandReuse, TheSeparatorsOfThePathsDoNotCount)
{
	auto lands = SameLandOfAFile();
	lands.openLand = "Online Maps\\mpm_2p_1.map";
	lands.openLandscape = "Data\\Landscape\\MP\\mpm_2p_1.lnd";
	EXPECT_TRUE(land_reuse::KeepsLand(lands));
}

TEST(LandReuse, ASavedGameBuildsTheLandAgain)
{
	auto lands = SameLandOfAFile();
	lands.worldState = false;
	EXPECT_FALSE(land_reuse::KeepsLand(lands));
}

TEST(LandReuse, NoOpenLandBuildsTheLand)
{
	auto lands = SameLandOfAFile();
	lands.landLoaded = false;
	EXPECT_FALSE(land_reuse::KeepsLand(lands));
}

TEST(LandReuse, AnotherLandBuildsTheLand)
{
	auto lands = SameLandOfAFile();
	lands.savedLand = "Online Maps/mpm_4p_1.map";
	EXPECT_FALSE(land_reuse::KeepsLand(lands));
}

TEST(LandReuse, AnotherLandscapeOfTheSameLandBuildsTheLand)
{
	auto lands = SameLandOfAFile();
	lands.savedLandscape = "Data/Landscape/MP/mpm_2p_2.lnd";
	EXPECT_FALSE(land_reuse::KeepsLand(lands));
}

TEST(LandReuse, ALandThatTheGameBuildsNeedsNoLandscape)
{
	// The open landscape is the landscape of an earlier land, because the game builds the map for 32 gods itself
	auto lands = SameLandOfAFile();
	lands.openLand = lands.savedLand = "obm_32p_1";
	lands.openLandscape = "Data/Landscape/Land1.lnd";
	lands.savedLandscape = "";
	lands.builtByGame = true;
	EXPECT_TRUE(land_reuse::KeepsLand(lands));
}

TEST(LandReuse, AWorldStateWithNoLandBuildsNothingOfTheOpenLand)
{
	auto lands = SameLandOfAFile();
	lands.openLand = lands.savedLand = "";
	EXPECT_FALSE(land_reuse::KeepsLand(lands));
}

TEST(LandReuse, AnUnchangedBlockHasNoChangedCells)
{
	const auto cells = Cells(20);
	const std::vector<uint8_t> built(cells.size(), 20);
	EXPECT_TRUE(land_reuse::ChangedCells(cells, built).empty());
}

TEST(LandReuse, TheChangedCellsComeInTheOrderOfTheBlock)
{
	auto cells = Cells(20);
	const std::vector<uint8_t> built(cells.size(), 20);
	// A corner of the 17th row, which is a copy of the first corner of the next block, counts too
	cells.at(16 * 17 + 3).altitude = 0;
	cells.at(5).altitude = 255;
	cells.at(17 + 1).altitude = 21;
	EXPECT_EQ(land_reuse::ChangedCells(cells, built), (std::vector<uint16_t> {5, 18, 275}));
}

TEST(LandReuse, TheChangedCornersOfABlockTakeTheirMapCells)
{
	auto cells = Cells(20);
	cells.at(5).altitude = 7;
	cells.at(16 * 17 + 3).altitude = 9;
	const std::vector<uint16_t> changed {5, 16 * 17 + 3};
	EXPECT_EQ(land_reuse::CornersOfBlock(32, 48, changed, cells),
	          (std::vector<land_reuse::CellAltitude> {{.x = 32, .z = 53, .altitude = 7}, {.x = 48, .z = 51, .altitude = 9}}));
}

TEST(LandReuse, TheAltitudesComeBackInTheOrderOfTheMapAndOnceForEachCorner)
{
	// The second block keeps a copy of the first corner of the third
	const std::vector<land_reuse::CellAltitude> altitudes {
	    {.x = 48, .z = 3, .altitude = 9}, {.x = 2, .z = 300, .altitude = 1}, {.x = 48, .z = 3, .altitude = 9}};
	const auto bytes = land_reuse::WriteAltitudes(altitudes);
	EXPECT_EQ(bytes.size(), 4u + 2u * 5u);
	const auto read = land_reuse::ReadAltitudes(bytes);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(*read,
	          (std::vector<land_reuse::CellAltitude> {{.x = 2, .z = 300, .altitude = 1}, {.x = 48, .z = 3, .altitude = 9}}));
}

TEST(LandReuse, NoChangedAltitudeIsOnlyTheCount)
{
	const auto bytes = land_reuse::WriteAltitudes({});
	EXPECT_EQ(bytes, (std::vector<uint8_t> {0, 0, 0, 0}));
	EXPECT_EQ(land_reuse::ReadAltitudes(bytes), std::vector<land_reuse::CellAltitude> {});
}

TEST(LandReuse, AltitudesThatAreNotWholeDoNotRead)
{
	auto bytes = land_reuse::WriteAltitudes({{.x = 1, .z = 2, .altitude = 3}});
	bytes.pop_back();
	EXPECT_FALSE(land_reuse::ReadAltitudes(bytes).has_value());
	EXPECT_FALSE(land_reuse::ReadAltitudes(std::vector<uint8_t> {1, 0}).has_value());
}
