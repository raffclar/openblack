/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>

#include <gtest/gtest.h>
#include <landgen/Swamps.h>

using namespace openblack::landgen;
using biomes::Biome;

namespace
{
/// A fake land of 2 km a side, with 3.8 square kilometres of land: grassland, with a square of swamp in the middle and the
/// sea on one side
struct SwampLand
{
	static constexpr int k_Side = 200;
	static constexpr int k_SwampLow = 50;
	static constexpr int k_SwampHigh = 110;
	Grid<Biome> biomes {k_Side, Biome::Grassland};
	Grid<float> heights {k_Side, 12.0f};
	Grid<float> sea {k_Side, 40.0f};
	Grid<uint8_t> keepClear {k_Side, 0};

	SwampLand()
	{
		for (int x = 0; x < k_Side; ++x)
		{
			for (int z = 0; z < k_Side; ++z)
			{
				if (x < 10)
				{
					biomes.At(x, z) = Biome::Sea;
					heights.At(x, z) = 0.0f;
					sea.At(x, z) = 0.0f;
				}
				else if (x >= k_SwampLow && x < k_SwampHigh && z >= k_SwampLow && z < k_SwampHigh)
				{
					biomes.At(x, z) = Biome::Swamp;
				}
			}
		}
	}

	[[nodiscard]] features::Ground Ground() const
	{
		return {.biomes = biomes,
		        .heights = heights,
		        .seaDistance = sea,
		        .keepClear = keepClear,
		        .clearings = {},
		        .townX = 20.0f,
		        .townZ = 20.0f};
	}
};

/// The sheets of the story only, with no cover
constexpr swamps::Settings k_SheetsOnly {.coverSpacing = 0.0f};

bool InSwamp(float x, float z)
{
	const auto i = static_cast<int>(std::lround(x / 10.0f));
	const auto j = static_cast<int>(std::lround(z / 10.0f));
	return i >= SwampLand::k_SwampLow && i < SwampLand::k_SwampHigh && j >= SwampLand::k_SwampLow && j < SwampLand::k_SwampHigh;
}
} // namespace

TEST(LandgenSwamps, ASheetHasThePuffsOfTheGreenMistOfTheStory)
{
	const SwampLand land;
	for (uint64_t seed = 1; seed <= 8; ++seed)
	{
		const auto result = swamps::Place(land.Ground(), seed, k_SheetsOnly);
		ASSERT_EQ(result.sheets.size(), 1u);
		ASSERT_EQ(result.mists.size(), 11u);
		EXPECT_EQ(swamps::PuffsInSheet(), 11u);
		uint32_t flat = 0;
		for (const auto& mist : result.mists)
		{
			EXPECT_EQ(mist.colour & 0xFFFFFFu, swamps::k_GreenMistColour);
			EXPECT_GE(mist.colour >> 24u, 0x7Eu);
			EXPECT_GE(mist.size, 0.65f);
			EXPECT_LE(mist.size, 1.92f);
			EXPECT_EQ(mist.altitude, 0.0f);
			flat += mist.edgeShrink > 1.0f ? 1u : 0u;
			EXPECT_TRUE(InSwamp(mist.x, mist.z)) << mist.x << ' ' << mist.z;
		}
		// The line of five flat puffs
		EXPECT_EQ(flat, 5u);
		// The first puff is the place of the sheet
		EXPECT_EQ(result.mists.front().x, result.sheets.front().x);
		EXPECT_EQ(result.mists.front().z, result.sheets.front().z);
	}
}

TEST(LandgenSwamps, NoSwampGivesNoMist)
{
	SwampLand land;
	std::ranges::replace(land.biomes.values, Biome::Swamp, Biome::Grassland);
	const auto result = swamps::Place(land.Ground(), 3, {});
	EXPECT_TRUE(result.sheets.empty());
	EXPECT_TRUE(result.mists.empty());
}

TEST(LandgenSwamps, ASwampTooSmallForTheSheetGivesNoMist)
{
	SwampLand land;
	// A swamp of 40 m a side: the sheet of the story is about 60 m across
	for (int x = 0; x < SwampLand::k_Side; ++x)
	{
		for (int z = 0; z < SwampLand::k_Side; ++z)
		{
			if (land.biomes.At(x, z) == Biome::Swamp && (x >= 54 || z >= 54))
			{
				land.biomes.At(x, z) = Biome::Grassland;
			}
		}
	}
	EXPECT_TRUE(swamps::Place(land.Ground(), 5, k_SheetsOnly).mists.empty());
}

TEST(LandgenSwamps, NoSheetGoesOverGroundThatRisesMoreThanUnderTheSheetOfTheStory)
{
	SwampLand land;
	// The swamp rises by 4 altitudes, or 2.7 m, for each corner. A sheet over it rises more than 12 m.
	for (int x = 0; x < SwampLand::k_Side; ++x)
	{
		for (int z = 0; z < SwampLand::k_Side; ++z)
		{
			land.heights.At(x, z) = 12.0f + (4.0f * static_cast<float>(x + z));
		}
	}
	EXPECT_TRUE(swamps::Place(land.Ground(), 2, k_SheetsOnly).mists.empty());
}

TEST(LandgenSwamps, TheSettingsLimitTheSheetsAndKeepThemApart)
{
	const SwampLand land;
	EXPECT_TRUE(swamps::Place(land.Ground(), 4, {.landPerSheet = 2.99f, .most = 0, .coverSpacing = 0.0f}).sheets.empty());
	const auto result = swamps::Place(land.Ground(), 4, {.landPerSheet = 0.5f, .most = 6, .coverSpacing = 0.0f});
	ASSERT_GE(result.sheets.size(), 1u);
	EXPECT_LE(result.sheets.size(), 6u);
	EXPECT_EQ(result.mists.size(), result.sheets.size() * swamps::PuffsInSheet());
	for (size_t a = 0; a < result.sheets.size(); ++a)
	{
		for (size_t b = a + 1; b < result.sheets.size(); ++b)
		{
			EXPECT_GE(std::hypot(result.sheets[a].x - result.sheets[b].x, result.sheets[a].z - result.sheets[b].z), 120.0f);
		}
	}
}

TEST(LandgenSwamps, TheSameSeedGivesTheSameSheets)
{
	const SwampLand land;
	const auto a = swamps::Place(land.Ground(), 11, {});
	const auto b = swamps::Place(land.Ground(), 11, {});
	ASSERT_EQ(a.mists.size(), b.mists.size());
	for (size_t i = 0; i < a.mists.size(); ++i)
	{
		EXPECT_EQ(a.mists[i].x, b.mists[i].x);
		EXPECT_EQ(a.mists[i].z, b.mists[i].z);
	}
}

TEST(LandgenSwamps, ALandHasOneSheetForEachAreaOfTheFourthLandOfTheStory)
{
	const swamps::Settings settings;
	EXPECT_EQ(swamps::SheetsFor(0.0f, settings), 0u);
	EXPECT_EQ(swamps::SheetsFor(2.5f, settings), 0u);
	EXPECT_EQ(swamps::SheetsFor(2.99f, settings), 1u);
	EXPECT_EQ(swamps::SheetsFor(13.0f, settings), 4u);
	// A land too small for a sheet has no mist, also with a swamp
	SwampLand land;
	for (int x = 0; x < SwampLand::k_Side; ++x)
	{
		for (int z = 0; z < SwampLand::k_Side; ++z)
		{
			if (x >= 160 || z >= 160)
			{
				land.biomes.At(x, z) = Biome::Sea;
			}
		}
	}
	EXPECT_TRUE(swamps::Place(land.Ground(), 1, k_SheetsOnly).mists.empty());
}

TEST(LandgenSwamps, TheCoverOfFlatPuffsGoesOverTheWholeSwamp)
{
	const SwampLand land;
	const swamps::Settings settings;
	const auto result = swamps::Place(land.Ground(), 6, settings);
	ASSERT_EQ(result.sheets.size(), 1u);
	const auto sheetPuffs = swamps::PuffsInSheet();
	ASSERT_EQ(result.mists.size(), sheetPuffs + result.coverPuffs);
	// The swamp is 600 m a side. Its middle of 580 m a side holds about 19 x 19 squares of 30 m, less the sheet.
	EXPECT_GE(result.coverPuffs, 300u);
	EXPECT_LE(result.coverPuffs, 400u);
	for (size_t i = sheetPuffs; i < result.mists.size(); ++i)
	{
		const auto& puff = result.mists[i];
		// The flat puff of the line of the story
		EXPECT_EQ(puff.colour, 0x8650632Fu);
		EXPECT_FLOAT_EQ(puff.size, 1.389350f);
		EXPECT_FLOAT_EQ(puff.edgeShrink, 3.780488f);
		EXPECT_TRUE(InSwamp(puff.x, puff.z)) << puff.x << ' ' << puff.z;
		// No puff of the cover is on a puff of the sheet
		for (size_t k = 0; k < sheetPuffs; ++k)
		{
			EXPECT_GE(std::hypot(puff.x - result.mists[k].x, puff.z - result.mists[k].z), 15.0f);
		}
	}
	// Every square of 60 m in the middle of the swamp has a puff
	for (int x = 560; x < 1040; x += 60)
	{
		for (int z = 560; z < 1040; z += 60)
		{
			EXPECT_TRUE(std::ranges::any_of(result.mists,
			                                [x, z](const auto& puff) {
				                                return puff.x >= static_cast<float>(x) && puff.x < static_cast<float>(x + 60) &&
				                                       puff.z >= static_cast<float>(z) && puff.z < static_cast<float>(z + 60);
			                                }))
			    << x << ' ' << z;
		}
	}
}

TEST(LandgenSwamps, TheCoverKeepsToItsMostPuffsAndOffSmallWetSpots)
{
	const SwampLand land;
	const auto capped = swamps::Place(land.Ground(), 6, {.mostCoverPuffs = 25});
	EXPECT_EQ(capped.coverPuffs, 25u);
	// A wet spot of one corner gets no cover
	SwampLand spots;
	std::ranges::replace(spots.biomes.values, Biome::Swamp, Biome::Grassland);
	for (int x = 20; x < 180; x += 3)
	{
		for (int z = 20; z < 180; z += 3)
		{
			spots.biomes.At(x, z) = Biome::Swamp;
		}
	}
	EXPECT_EQ(swamps::Place(spots.Ground(), 6, {}).coverPuffs, 0u);
}
