/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <numbers>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Graphics/CloudRegions.h"

namespace regions = openblack::graphics::cloud_regions;

TEST(CloudRegions, IslandKeepsItsOwnWeather)
{
	EXPECT_EQ(regions::RegionShare(0.0f), 0.0f);
	EXPECT_EQ(regions::RegionShare(regions::k_KeepNear), 0.0f);
	EXPECT_EQ(regions::RegionShare(regions::k_KeepFar), 1.0f);
	EXPECT_GT(regions::RegionShare(20000.0f), 0.2f);
	EXPECT_LT(regions::RegionShare(20000.0f), 0.8f);
	// Near the island the cover does not change, whatever the billows are
	EXPECT_EQ(regions::CoverShare(0.3f, 0.0f), 1.0f);
	EXPECT_EQ(regions::CoverShare(-0.3f, 0.0f), 1.0f);
}

TEST(CloudRegions, RegionsHaveClearAndCloudyAreas)
{
	EXPECT_LT(regions::CoverShare(-0.3f, 1.0f), 0.35f);
	EXPECT_GT(regions::CoverShare(-1.0f, 1.0f), 0.0f);
	EXPECT_GT(regions::CoverShare(0.3f, 1.0f), 1.6f);
	EXPECT_LE(regions::CoverShare(1.0f, 1.0f), 2.5f);
}

TEST(CloudRegions, SizeWeightsShareBetweenNeighbours)
{
	EXPECT_EQ(regions::SizeWeights(0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_EQ(regions::SizeWeights(0.5f), glm::vec3(0.0f, 1.0f, 0.0f));
	EXPECT_EQ(regions::SizeWeights(1.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	for (float size = -0.2f; size <= 1.2f; size += 0.05f)
	{
		const glm::vec3 w = regions::SizeWeights(size);
		EXPECT_NEAR(w.x + w.y + w.z, 1.0f, 1e-6f) << size;
		// The small and the large cells never mix directly
		EXPECT_EQ(w.x * w.z, 0.0f) << size;
	}
	EXPECT_LT(regions::CellWidth(regions::SizeWeights(0.0f)), 1000.0f);
	EXPECT_GT(regions::CellWidth(regions::SizeWeights(1.0f)), 5000.0f);
}

TEST(CloudRegions, MixKeepsTheContrastOfOnePattern)
{
	// One pattern alone is not changed
	EXPECT_NEAR(regions::MixPatterns({0.8f, 0.1f, 0.3f}, {1.0f, 0.0f, 0.0f}), 0.8f, 1e-6f);
	// An equal mix of two patterns has the spread of one, not half of it
	const float a = regions::MixPatterns({0.8f, 0.8f, 0.5f}, {0.5f, 0.5f, 0.0f});
	EXPECT_GT(a, 0.9f);
	EXPECT_NEAR(regions::MixPatterns({0.5f, 0.5f, 0.5f}, {0.3f, 0.7f, 0.0f}), 0.5f, 1e-6f);
	EXPECT_EQ(regions::MixPatterns({0.9f, 0.9f, 0.9f}, {0.0f, 0.0f, 0.0f}), 0.5f);
	EXPECT_LE(regions::MixPatterns({1.0f, 1.0f, 1.0f}, {0.5f, 0.5f, 0.0f}), 1.0f);
}

TEST(CloudRegions, FarEdgesAreSoft)
{
	const float width = regions::CellWidth(regions::SizeWeights(0.5f));
	EXPECT_EQ(regions::EdgeSoftness(0.0f, width), 0.0f);
	EXPECT_LT(regions::EdgeSoftness(20.0f, width), 0.02f);
	EXPECT_GT(regions::EdgeSoftness(400.0f, width), regions::EdgeSoftness(200.0f, width));
	EXPECT_EQ(regions::EdgeSoftness(1e6f, width), 1.5f);
	// A pixel softens small cells more than large ones
	EXPECT_GT(regions::EdgeSoftness(200.0f, 800.0f), regions::EdgeSoftness(200.0f, 5000.0f));
}

TEST(CloudRegions, OpenCellsHaveRingsRoundClearMiddles)
{
	EXPECT_EQ(regions::OpenRing(0.5f), 1.0f);
	EXPECT_EQ(regions::OpenRing(1.0f), 0.0f);
	EXPECT_EQ(regions::OpenRing(0.0f), 0.0f);
	EXPECT_GT(regions::OpenRing(0.55f), 0.5f);
	EXPECT_NEAR(regions::OpenRing(0.4f), regions::OpenRing(0.6f), 1e-6f);
}

namespace
{
/// The weight of each fixed axis of the streets for a direction
std::array<float, regions::k_StreetAxes> AxisWeights(float angle)
{
	const auto blend = regions::StreetAxes(angle);
	std::array<float, regions::k_StreetAxes> weights {};
	weights.at(blend.first) += 1.0f - blend.share;
	weights.at(blend.second) += blend.share;
	return weights;
}
} // namespace

TEST(CloudRegions, StreetsLieAlongTheWind)
{
	constexpr float k_Pi = std::numbers::pi_v<float>;
	const auto east = regions::StreetAxes(0.0f);
	EXPECT_EQ(east.first, 0);
	EXPECT_NEAR(east.share, 0.0f, 1e-6f);
	const auto between = regions::StreetAxes(2.5f * k_Pi / static_cast<float>(regions::k_StreetAxes));
	EXPECT_EQ(between.first, 2);
	EXPECT_EQ(between.second, 3);
	EXPECT_NEAR(between.share, 0.5f, 1e-4f);
}

TEST(CloudRegions, StreetsTurnSmoothlyWithTheWind)
{
	constexpr float k_Pi = std::numbers::pi_v<float>;
	auto last = AxisWeights(-2.0f * k_Pi);
	for (int i = 1; i <= 4000; ++i)
	{
		const float angle = -2.0f * k_Pi + (4.0f * k_Pi * static_cast<float>(i) / 4000.0f);
		const auto now = AxisWeights(angle);
		float change = 0.0f;
		for (int k = 0; k < regions::k_StreetAxes; ++k)
		{
			change += std::abs(now.at(k) - last.at(k));
		}
		// A step of 1/500 of a half turn moves at most 2 x 8/500 of the weight
		EXPECT_LT(change, 0.04f) << angle;
		last = now;
	}
	// A street has no front and back
	const auto ahead = AxisWeights(0.7f);
	const auto behind = AxisWeights(0.7f + k_Pi);
	for (int k = 0; k < regions::k_StreetAxes; ++k)
	{
		EXPECT_NEAR(ahead.at(k), behind.at(k), 1e-4f);
	}
}

TEST(CloudRegions, StreetAxesGoRoundAHalfTurn)
{
	const auto first = regions::StreetAxisDirection(0);
	const auto again = regions::StreetAxisDirection(regions::k_StreetAxes);
	EXPECT_NEAR(glm::length(first - again), 0.0f, 1e-6f);
	EXPECT_NEAR(glm::length(regions::StreetAxisDirection(-1) - regions::StreetAxisDirection(regions::k_StreetAxes - 1)), 0.0f,
	            1e-6f);
	EXPECT_NEAR(glm::length(regions::StreetAxisDirection(3)), 1.0f, 1e-6f);
}

TEST(CloudRegions, FarSheetDetailShowsOnlyWherePixelsAreSmall)
{
	constexpr float k_Tile = 34000.0f;
	EXPECT_FLOAT_EQ(regions::SheetDetailWeight(100.0f, k_Tile), 1.0f);
	EXPECT_FLOAT_EQ(regions::SheetDetailWeight(5000.0f, k_Tile), 0.0f);
	EXPECT_NEAR(regions::SheetDetailWeight(1000.0f, k_Tile), 1.0f, 1e-6f);
	float last = 1.0f;
	for (int i = 0; i < 100; ++i)
	{
		const float weight = regions::SheetDetailWeight(100.0f + (50.0f * static_cast<float>(i)), k_Tile);
		EXPECT_LE(weight, last);
		last = weight;
	}
	// A finer read goes out of view nearer than a coarse read
	EXPECT_LT(regions::SheetDetailWeight(1200.0f, 11000.0f), regions::SheetDetailWeight(1200.0f, k_Tile));
}
