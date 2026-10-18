/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

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
