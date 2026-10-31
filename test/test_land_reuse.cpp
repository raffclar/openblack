/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <vector>

#include <LNDFile.h>
#include <WorldFile.h>
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

/// A block of land at the map cell (32, 48), as the game built it at altitude 20
struct FakeBlock
{
	static constexpr uint16_t k_FirstX = 32;
	static constexpr uint16_t k_FirstZ = 48;
	std::array<lnd::LNDCell, 17 * 17> cells = Cells(20);
	std::vector<uint8_t> built = std::vector<uint8_t>(17 * 17, 20);

	/// The corners whose altitude is not the altitude of the land as the game built it
	[[nodiscard]] std::vector<land_reuse::CellAltitude> Changed() const
	{
		return land_reuse::CornersOfBlock(k_FirstX, k_FirstZ, land_reuse::ChangedCells(cells, built), cells);
	}

	/// Puts each kept altitude back on its corner, as a load does
	void Put(std::span<const land_reuse::CellAltitude> altitudes)
	{
		for (const auto& corner : altitudes)
		{
			const auto x = static_cast<size_t>(corner.x - k_FirstX);
			const auto z = static_cast<size_t>(corner.z - k_FirstZ);
			cells.at(x * 17 + z).altitude = corner.altitude;
		}
	}
};

/// A vortex levels a patch of 3 by 3 corners to the altitude 77
FakeBlock LevelledBlock()
{
	FakeBlock block;
	for (size_t x = 4; x < 7; ++x)
	{
		for (size_t z = 8; z < 11; ++z)
		{
			block.cells.at(x * 17 + z).altitude = 77;
		}
	}
	return block;
}

/// The file of a saved game or of a world state: the game block, then the block of the changed altitudes
gamesave::WorldFile FileWith(const gamesave::GameChunk& game, const FakeBlock& land)
{
	gamesave::WorldFile file;
	file.chunks.push_back(gamesave::ToChunk(game));
	file.chunks.push_back(land_reuse::AltitudesChunk(land.Changed()));
	return file;
}

/// Writes the file, reads it again and puts its kept altitudes on a block that the game built again
FakeBlock LoadedBlock(const gamesave::WorldFile& file)
{
	const auto read = gamesave::ReadWorldFile(gamesave::Write(file));
	EXPECT_TRUE(read.has_value());
	FakeBlock loaded;
	if (!read.has_value())
	{
		return loaded;
	}
	const auto altitudes = land_reuse::KeptAltitudes(*read);
	EXPECT_TRUE(altitudes.has_value());
	if (altitudes.has_value())
	{
		loaded.Put(*altitudes);
	}
	return loaded;
}

bool SameAltitudes(const FakeBlock& a, const FakeBlock& b)
{
	return std::ranges::equal(a.cells, b.cells, {}, &lnd::LNDCell::altitude, &lnd::LNDCell::altitude);
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

TEST(LandReuse, ALevelledPatchComesBackAfterASaveAndALoadOfASavedGame)
{
	const auto levelled = LevelledBlock();
	ASSERT_EQ(levelled.Changed().size(), 9u);
	const gamesave::GameChunk game {.land = 2,
	                                .turn = 500,
	                                .landscape = "Scripts/Land2.txt",
	                                .knownLeashes = 0,
	                                .creatureInTemple = 0,
	                                .lastAutosaveTurn = 0,
	                                .lastQuickSaveTurn = 0,
	                                .landscapeFile = "Data/Landscape/Land2.lnd"};
	const auto loaded = LoadedBlock(FileWith(game, levelled));
	EXPECT_TRUE(SameAltitudes(loaded, levelled));
}

TEST(LandReuse, ALevelledPatchComesBackAfterASaveAndALoadOfAWorldState)
{
	const auto levelled = LevelledBlock();
	const gamesave::GameChunk game {.land = 0,
	                                .turn = 1200,
	                                .landscape = "Online Maps/mpm_2p_1.map",
	                                .knownLeashes = 0,
	                                .creatureInTemple = 0,
	                                .lastAutosaveTurn = 900,
	                                .lastQuickSaveTurn = 0,
	                                .landscapeFile = "Data/Landscape/MP/mpm_2p_1.lnd"};
	const auto loaded = LoadedBlock(FileWith(game, levelled));
	EXPECT_TRUE(SameAltitudes(loaded, levelled));
}

TEST(LandReuse, AFileWithNoAltitudesStillLoadsWithTheLandAsTheGameBuiltIt)
{
	gamesave::WorldFile old;
	old.chunks.push_back(gamesave::ToChunk(gamesave::GameChunk {.land = 2,
	                                                            .turn = 500,
	                                                            .landscape = "Scripts/Land2.txt",
	                                                            .knownLeashes = 0,
	                                                            .creatureInTemple = 0,
	                                                            .lastAutosaveTurn = 0,
	                                                            .lastQuickSaveTurn = 0,
	                                                            .landscapeFile = "Data/Landscape/Land2.lnd"}));
	const auto read = gamesave::ReadWorldFile(gamesave::Write(old));
	ASSERT_TRUE(read.has_value());
	const auto altitudes = land_reuse::KeptAltitudes(*read);
	ASSERT_TRUE(altitudes.has_value());
	EXPECT_TRUE(altitudes->empty());
	EXPECT_TRUE(SameAltitudes(LoadedBlock(old), FakeBlock {}));
}

TEST(LandReuse, ABlockOfAltitudesThatIsNotWholeGivesNothing)
{
	auto chunk = land_reuse::AltitudesChunk({{.x = 1, .z = 2, .altitude = 3}});
	chunk.bytes.pop_back();
	gamesave::WorldFile file;
	file.chunks.push_back(chunk);
	EXPECT_FALSE(land_reuse::KeptAltitudes(file).has_value());
}
