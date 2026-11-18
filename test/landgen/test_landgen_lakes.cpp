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
#include <landgen/Hydrology.h>
#include <landgen/Lakes.h>

using namespace openblack::landgen;

namespace
{
constexpr int k_Side = 64;
constexpr float k_SeaLevel = 3.0f;
/// The filled heights rise a very small step from corner to corner across flat ground
constexpr float k_FlatRiseError = 0.05f;

/// Flat ground at this height
Grid<float> Plateau(float height)
{
	return {k_Side, height};
}

/// Digs a round bowl of this radius and depth at its middle, under the ground round it
void Bowl(Grid<float>& heights, float cx, float cz, float radius, float depth)
{
	for (int x = 0; x < k_Side; ++x)
	{
		for (int z = 0; z < k_Side; ++z)
		{
			const float dx = static_cast<float>(x) - cx;
			const float dz = static_cast<float>(z) - cz;
			const float part = ((dx * dx) + (dz * dz)) / (radius * radius);
			if (part < 1.0f)
			{
				auto& height = heights.At(x, z);
				height = std::min(height, height - (depth * (1.0f - part)));
			}
		}
	}
}

bool Holds(const lakes::Lake& lake, const Grid<float>& heights, int x, int z)
{
	return std::ranges::binary_search(lake.corners, static_cast<uint32_t>(heights.Index(x, z)));
}
} // namespace

TEST(LandgenLakes, ABowlOnAHillHoldsALakeJustUnderItsRim)
{
	auto heights = Plateau(40.0f);
	Bowl(heights, 32.0f, 32.0f, 8.0f, 5.0f);
	const lakes::Settings settings;
	const auto found = lakes::Find(heights, k_SeaLevel, settings);
	ASSERT_EQ(found.size(), 1u);
	EXPECT_NEAR(found[0].level, 40.0f - settings.freeboard, k_FlatRiseError);
	EXPECT_TRUE(Holds(found[0], heights, 32, 32));
	// The shore next to the water is part of the lake, the ground far from it is not
	EXPECT_TRUE(Holds(found[0], heights, 40, 32));
	EXPECT_FALSE(Holds(found[0], heights, 50, 32));
	EXPECT_FALSE(Holds(found[0], heights, 2, 2));
}

TEST(LandgenLakes, ASmallOrShallowBasinHoldsNoLake)
{
	auto small = Plateau(40.0f);
	Bowl(small, 32.0f, 32.0f, 2.5f, 5.0f);
	EXPECT_TRUE(lakes::Find(small, k_SeaLevel, {}).empty());

	auto shallow = Plateau(40.0f);
	Bowl(shallow, 32.0f, 32.0f, 8.0f, 1.5f);
	EXPECT_TRUE(lakes::Find(shallow, k_SeaLevel, {}).empty());
}

TEST(LandgenLakes, TwoLakesApartStayTwo)
{
	auto heights = Plateau(40.0f);
	Bowl(heights, 14.0f, 32.0f, 8.0f, 5.0f);
	Bowl(heights, 50.0f, 32.0f, 8.0f, 5.0f);
	const auto found = lakes::Find(heights, k_SeaLevel, {});
	ASSERT_EQ(found.size(), 2u);
	for (const auto corner : found[0].corners)
	{
		EXPECT_FALSE(std::ranges::binary_search(found[1].corners, corner));
	}
}

TEST(LandgenLakes, TwoTouchingBowlsGiveOneLakeAtOneLevel)
{
	auto heights = Plateau(40.0f);
	Bowl(heights, 20.0f, 32.0f, 8.0f, 5.0f);
	Bowl(heights, 37.0f, 32.0f, 8.0f, 5.0f);
	const auto found = lakes::Find(heights, k_SeaLevel, {});
	ASSERT_EQ(found.size(), 1u);
	EXPECT_TRUE(Holds(found[0], heights, 20, 32));
	EXPECT_TRUE(Holds(found[0], heights, 37, 32));
}

TEST(LandgenLakes, ALakeThatTouchesALowerLakeJoinsItAtTheLowerLevel)
{
	// The second bowl is on a low step of the hill, half a unit higher. The shores of the two lakes share the step.
	auto heights = Plateau(40.0f);
	for (int x = 29; x < k_Side; ++x)
	{
		for (int z = 0; z < k_Side; ++z)
		{
			heights.At(x, z) = 40.5f;
		}
	}
	Bowl(heights, 20.0f, 32.0f, 8.0f, 5.0f);
	Bowl(heights, 37.0f, 32.0f, 7.0f, 5.0f);
	const lakes::Settings settings;
	const auto found = lakes::Find(heights, k_SeaLevel, settings);
	ASSERT_EQ(found.size(), 1u);
	const float level = 40.0f - settings.freeboard;
	EXPECT_NEAR(found[0].level, level, k_FlatRiseError);
	// The bed of the higher bowl went down under the one level, so that its water stays
	EXPECT_LT(heights.At(37, 32), level);
	EXPECT_LT(heights.At(37, 28), level);
	EXPECT_TRUE(Holds(found[0], heights, 37, 32));
}

TEST(LandgenLakes, ALakeOnACliffTopAndALakeAtItsFootStayTwo)
{
	// The high ground ends at a cliff 30 high. A bowl lies at its top edge, and a bowl lies at its foot.
	auto heights = Plateau(10.0f);
	for (int x = 32; x < k_Side; ++x)
	{
		for (int z = 0; z < k_Side; ++z)
		{
			heights.At(x, z) = 40.0f;
		}
	}
	Bowl(heights, 40.0f, 32.0f, 8.0f, 5.0f);
	Bowl(heights, 24.0f, 32.0f, 8.0f, 5.0f);
	const auto found = lakes::Find(heights, k_SeaLevel, {});
	ASSERT_EQ(found.size(), 2u);
	EXPECT_GT(std::abs(found[0].level - found[1].level), 20.0f);
	// No bed went down
	EXPECT_GT(heights.At(40, 28), 30.0f);
}

TEST(LandgenLakes, ASmallLakeOnTheShoreOfALakeJoinsIt)
{
	auto heights = Plateau(40.0f);
	Bowl(heights, 20.0f, 32.0f, 10.0f, 5.0f);
	Bowl(heights, 33.0f, 32.0f, 2.5f, 4.0f);
	lakes::Settings settings;
	settings.minCorners = 8;
	const auto found = lakes::Find(heights, k_SeaLevel, settings);
	ASSERT_EQ(found.size(), 1u);
	EXPECT_TRUE(Holds(found[0], heights, 33, 32));
	EXPECT_NEAR(found[0].level, 40.0f - settings.freeboard, k_FlatRiseError);
}

TEST(LandgenLakes, LandAtTheSeaLevelHoldsNoLake)
{
	auto heights = Plateau(k_SeaLevel);
	Bowl(heights, 32.0f, 32.0f, 8.0f, 5.0f);
	EXPECT_TRUE(lakes::Find(heights, k_SeaLevel, {}).empty());
}

TEST(LandgenLakes, HollowsGoOnlyOnHighFlatGround)
{
	auto high = Plateau(50.0f);
	const Grid<uint8_t> nothingToKeep(k_Side, 0);
	lakes::DigSettings settings;
	settings.count = 1;
	lakes::DigHollows(high, nothingToKeep, {}, 7, settings);
	const auto found = lakes::Find(high, k_SeaLevel, {});
	ASSERT_EQ(found.size(), 1u);
	EXPECT_NEAR(found[0].level, 50.0f - lakes::Settings {}.freeboard, k_FlatRiseError);

	auto low = Plateau(10.0f);
	lakes::DigHollows(low, nothingToKeep, {}, 7, settings);
	EXPECT_TRUE(std::ranges::all_of(low.values, [](float height) { return height == 10.0f; }));
}

TEST(LandgenLakes, HollowsKeepAwayFromTheMarkedGround)
{
	auto heights = Plateau(50.0f);
	const Grid<uint8_t> everything(k_Side, 1);
	lakes::DigHollows(heights, everything, {}, 7, {});
	EXPECT_TRUE(std::ranges::all_of(heights.values, [](float height) { return height == 50.0f; }));
}

TEST(LandgenLakes, ALargeDeepHighBasinKeepsItsHollow)
{
	auto heights = Plateau(40.0f);
	Bowl(heights, 32.0f, 32.0f, 8.0f, 5.0f);
	const auto before = heights;
	const auto drainage = hydrology::Drain(heights, k_SeaLevel);
	const auto basins = hydrology::FormLakes(heights, drainage, {});
	EXPECT_EQ(heights.values, before.values);
	EXPECT_TRUE(basins.seaLakes.empty());

	auto small = Plateau(40.0f);
	Bowl(small, 32.0f, 32.0f, 3.0f, 5.0f);
	const auto smallDrainage = hydrology::Drain(small, k_SeaLevel);
	const auto smallBasins = hydrology::FormLakes(small, smallDrainage, {});
	// A small basin fills with ground and stays wet
	EXPECT_GT(small.At(32, 32), 39.0f);
	EXPECT_EQ(smallBasins.wet.At(32, 32), 1);
}
