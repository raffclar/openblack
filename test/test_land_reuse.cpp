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
