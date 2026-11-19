/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <gtest/gtest.h>
#include <landgen/LakeShore.h>

using namespace openblack::landgen;
using lakeshore::Shore;

namespace
{
constexpr int k_Side = 40;
constexpr float k_Warm = 0.6f;

/// A fake land: flat ground, warm, gentle, with the same beach width everywhere
struct Fake
{
	Grid<float> altitudes;
	Grid<float> slopes {k_Side, 0.5f};
	Grid<float> temperature {k_Side, k_Warm};
	Grid<float> widths {k_Side, 6.0f};

	explicit Fake(float ground)
	    : altitudes(k_Side, ground)
	{
	}

	[[nodiscard]] lakeshore::Ground Ground() const
	{
		return {.altitudes = altitudes, .slopes = slopes, .temperature = temperature, .beachWidths = widths};
	}
};

/// A square lake of water from the low corner to the high corner, at its level, with the bed one unit under the level
lakes::Lake SquareLake(Fake& fake, int low, int high, float level)
{
	lakes::Lake lake {.level = level, .corners = {}};
	for (int x = low; x <= high; ++x)
	{
		for (int z = low; z <= high; ++z)
		{
			fake.altitudes.At(x, z) = level - 1.0f;
			lake.corners.push_back(static_cast<uint32_t>(fake.altitudes.Index(x, z)));
		}
	}
	return lake;
}
} // namespace

TEST(LandgenLakeShore, NoLakeGivesNoEdge)
{
	const Fake fake(60.0f);
	const auto shores = lakeshore::Find(fake.Ground(), {}, {});
	for (const auto shore : shores.shore.values)
	{
		EXPECT_EQ(shore, Shore::Land);
	}
}

TEST(LandgenLakeShore, TheLakeIsBedAndTheGentleGroundRoundItIsBeachToTheWidth)
{
	Fake fake(60.5f);
	const std::vector<lakes::Lake> lakes {SquareLake(fake, 15, 20, 60.0f)};
	const auto shores = lakeshore::Find(fake.Ground(), lakes, {});
	EXPECT_EQ(shores.shore.At(17, 17), Shore::Bed);
	EXPECT_FLOAT_EQ(shores.distance.At(17, 17), 0.0f);
	// The width is 6 corners from the water
	EXPECT_EQ(shores.shore.At(21, 17), Shore::Beach);
	EXPECT_EQ(shores.shore.At(26, 17), Shore::Beach);
	EXPECT_EQ(shores.shore.At(27, 17), Shore::Land);
	EXPECT_EQ(shores.shore.At(17, 8), Shore::Land);
	EXPECT_EQ(shores.shore.At(17, 9), Shore::Beach);
}

TEST(LandgenLakeShore, TheBeachHeightComesFromTheLevelOfTheLake)
{
	// The sea beach goes up to 13 altitudes over the drawn sea (16 less the sea level of 3)
	Fake fake(60.5f);
	const std::vector<lakes::Lake> lakes {SquareLake(fake, 15, 20, 60.0f)};
	fake.altitudes.At(22, 17) = 72.9f;
	fake.altitudes.At(23, 17) = 73.1f;
	fake.altitudes.At(17, 22) = 59.5f;
	const auto shores = lakeshore::Find(fake.Ground(), lakes, {});
	EXPECT_EQ(shores.shore.At(22, 17), Shore::Beach);
	EXPECT_EQ(shores.shore.At(23, 17), Shore::Land);
	// Ground under the level of the lake but out of the lake is not its beach
	EXPECT_EQ(shores.shore.At(17, 22), Shore::Land);
}

TEST(LandgenLakeShore, SteepOrColdGroundNearTheWaterIsWetSand)
{
	Fake fake(60.5f);
	const std::vector<lakes::Lake> lakes {SquareLake(fake, 15, 20, 60.0f)};
	fake.slopes.At(21, 17) = 4.0f;
	fake.temperature.At(17, 21) = 0.35f;
	fake.slopes.At(25, 17) = 4.0f;
	const auto shores = lakeshore::Find(fake.Ground(), lakes, {});
	EXPECT_EQ(shores.shore.At(21, 17), Shore::Bed);
	EXPECT_EQ(shores.shore.At(17, 21), Shore::Bed);
	EXPECT_EQ(shores.shore.At(14, 17), Shore::Beach);
	// The wet sand stops after the corners of the shore steps of the sea
	EXPECT_EQ(shores.shore.At(25, 17), Shore::Land);
}

TEST(LandgenLakeShore, TheWetSandGoesUpToTheTopOfTheWetSandOfTheCoast)
{
	// The wet sand of the coast goes up to altitude 8, which is 5 over the drawn sea
	Fake fake(60.5f);
	const std::vector<lakes::Lake> lakes {SquareLake(fake, 15, 20, 60.0f)};
	fake.slopes = Grid<float>(k_Side, 6.0f);
	fake.altitudes.At(22, 17) = 64.9f;
	fake.altitudes.At(23, 17) = 65.1f;
	const auto shores = lakeshore::Find(fake.Ground(), lakes, {});
	EXPECT_EQ(shores.shore.At(22, 17), Shore::Bed);
	EXPECT_EQ(shores.shore.At(23, 17), Shore::Land);
}

TEST(LandgenLakeShore, TheBeachWidthFollowsTheWidthOfEachCorner)
{
	Fake fake(60.5f);
	const std::vector<lakes::Lake> lakes {SquareLake(fake, 15, 20, 60.0f)};
	for (int z = 0; z < k_Side; ++z)
	{
		for (int x = 21; x < k_Side; ++x)
		{
			fake.widths.At(x, z) = 12.0f;
		}
	}
	const auto shores = lakeshore::Find(fake.Ground(), lakes, {});
	EXPECT_EQ(shores.shore.At(32, 17), Shore::Beach);
	EXPECT_EQ(shores.shore.At(33, 17), Shore::Land);
	EXPECT_EQ(shores.shore.At(8, 17), Shore::Land);
}

TEST(LandgenLakeShore, EachCornerUsesTheLevelOfTheNearestLake)
{
	// A low lake at 20 and a high lake at 60 on ground that rises between them
	Fake fake(30.0f);
	const std::vector<lakes::Lake> lakes {SquareLake(fake, 2, 6, 20.0f), SquareLake(fake, 30, 34, 60.0f)};
	fake.altitudes.At(8, 4) = 25.0f;
	fake.altitudes.At(28, 32) = 65.0f;
	// Nearer to the low lake, but too high over its level
	fake.altitudes.At(4, 9) = 40.0f;
	const auto shores = lakeshore::Find(fake.Ground(), lakes, {});
	EXPECT_EQ(shores.shore.At(8, 4), Shore::Beach);
	EXPECT_EQ(shores.shore.At(28, 32), Shore::Beach);
	EXPECT_EQ(shores.shore.At(4, 9), Shore::Land);
}

TEST(LandgenLakeShore, AlpineGroundHasNoBeach)
{
	Fake fake(140.5f);
	const std::vector<lakes::Lake> lakes {SquareLake(fake, 15, 20, 140.0f)};
	const auto shores = lakeshore::Find(fake.Ground(), lakes, {});
	EXPECT_EQ(shores.shore.At(17, 17), Shore::Bed);
	EXPECT_EQ(shores.shore.At(21, 17), Shore::Bed);
	EXPECT_EQ(shores.shore.At(26, 17), Shore::Land);
}

TEST(LandgenLakeShore, NoLakeGivesNoShoreSteps)
{
	Fake fake(60.0f);
	const auto lowered = lakeshore::ShoreSteps(fake.altitudes, {}, {}, {});
	EXPECT_TRUE(lowered.empty());
	EXPECT_FLOAT_EQ(fake.altitudes.At(10, 10), 60.0f);
}

TEST(LandgenLakeShore, AWallRoundALakeGetsTheShoreStepsOfTheSea)
{
	// A lake at 59.75 with its shore at 60, in a wall 30 altitudes over the shore
	Fake fake(90.0f);
	std::vector<lakes::Lake> lakes {SquareLake(fake, 15, 20, 59.75f)};
	const auto lowered = lakeshore::ShoreSteps(fake.altitudes, lakes, {}, {.freeboard = 0.25f, .rise = 28.0f});
	EXPECT_FALSE(lowered.empty());
	// The first three corners are at the shore, the fourth is one altitude over it
	EXPECT_FLOAT_EQ(fake.altitudes.At(21, 17), 60.0f);
	EXPECT_FLOAT_EQ(fake.altitudes.At(22, 17), 60.0f);
	EXPECT_FLOAT_EQ(fake.altitudes.At(23, 17), 60.2f);
	EXPECT_FLOAT_EQ(fake.altitudes.At(24, 17), 61.0f);
	// Behind the steps the wall rises by the rise of the settings for each corner, to the old ground
	EXPECT_FLOAT_EQ(fake.altitudes.At(25, 17), 89.0f);
	EXPECT_FLOAT_EQ(fake.altitudes.At(26, 17), 90.0f);
	// The corners of the lake do not change
	EXPECT_FLOAT_EQ(fake.altitudes.At(17, 17), 58.75f);
}

TEST(LandgenLakeShore, TheShoreStepsDoNotLiftLowGround)
{
	Fake fake(60.0f);
	std::vector<lakes::Lake> lakes {SquareLake(fake, 15, 20, 59.75f)};
	fake.altitudes.At(23, 17) = 60.1f;
	const auto lowered = lakeshore::ShoreSteps(fake.altitudes, lakes, {}, {});
	EXPECT_TRUE(lowered.empty());
	EXPECT_FLOAT_EQ(fake.altitudes.At(23, 17), 60.1f);
	EXPECT_FLOAT_EQ(fake.altitudes.At(24, 17), 60.0f);
}

TEST(LandgenLakeShore, TheShoreStepsOfALowLakeDoNotCutTheRimOfAHighLake)
{
	// A low lake and a high lake on a high ridge, with three corners between them on the diagonal
	Fake fake(80.0f);
	std::vector<lakes::Lake> lakes {SquareLake(fake, 5, 10, 39.75f), SquareLake(fake, 14, 19, 69.75f)};
	const auto lowered = lakeshore::ShoreSteps(fake.altitudes, lakes, {}, {});
	EXPECT_FALSE(lowered.empty());
	// The corners near the high lake keep at least its shore
	EXPECT_FLOAT_EQ(fake.altitudes.At(12, 12), 70.0f);
	EXPECT_FLOAT_EQ(fake.altitudes.At(13, 13), 70.0f);
	// The other corners get the shore steps of the low lake
	EXPECT_FLOAT_EQ(fake.altitudes.At(11, 11), 40.0f);
	EXPECT_FLOAT_EQ(fake.altitudes.At(7, 2), 40.2f);
}

TEST(LandgenLakeShore, TheShoreStepsKeepTheGroundOfThePlacesToKeep)
{
	Fake fake(90.0f);
	std::vector<lakes::Lake> lakes {SquareLake(fake, 15, 20, 59.75f)};
	const std::vector<lakes::Keep> keep {{.x = 23.0f, .z = 17.0f, .radius = 1.0f}};
	const auto lowered = lakeshore::ShoreSteps(fake.altitudes, lakes, keep, {});
	EXPECT_FALSE(lowered.empty());
	EXPECT_FLOAT_EQ(fake.altitudes.At(23, 17), 90.0f);
	EXPECT_FLOAT_EQ(fake.altitudes.At(22, 17), 90.0f);
	EXPECT_FLOAT_EQ(fake.altitudes.At(21, 17), 60.0f);
	EXPECT_FLOAT_EQ(fake.altitudes.At(23, 15), 60.2f);
}
