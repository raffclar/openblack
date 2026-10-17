/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the volumetric fog

#include <vector>

#include <gtest/gtest.h>

#include "Graphics/VolumetricFog.h"

namespace fog = openblack::graphics::volumetric_fog;

TEST(VolumetricFog, TheSlicesGoFartherApartAndInvert)
{
	EXPECT_NEAR(fog::SliceAt(fog::SliceDistance(0.0f)), 0.0f, 1e-4f);
	EXPECT_NEAR(fog::SliceAt(fog::SliceDistance(17.3f)), 17.3f, 1e-3f);
	// The far edge of the last slice is the farthest distance
	EXPECT_NEAR(fog::SliceDistance(static_cast<float>(fog::k_Slices) - 0.5f), fog::k_Farthest, 0.1f);
	// Far slices are thicker than near ones
	const float nearThickness = fog::SliceDistance(1.0f) - fog::SliceDistance(0.0f);
	const float farThickness = fog::SliceDistance(40.0f) - fog::SliceDistance(39.0f);
	EXPECT_GT(farThickness, nearThickness * 10.0f);
}

TEST(VolumetricFog, TheGroundFogComesAtDawnAndBurnsOff)
{
	// Thickest before and round sunrise, gone in the afternoon, a little at night
	EXPECT_GT(fog::DawnFog(5.5f), 0.9f);
	EXPECT_GT(fog::DawnFog(7.0f), 0.8f);
	EXPECT_LT(fog::DawnFog(9.5f), fog::DawnFog(8.0f));
	EXPECT_EQ(fog::DawnFog(14.0f), 0.0f);
	EXPECT_GT(fog::DawnFog(23.5f), 0.2f);
	EXPECT_LT(fog::DawnFog(23.5f), 0.5f);
	// It lifts as it thins
	EXPECT_GT(fog::FogTop(10.0f), fog::FogTop(6.0f) + 30.0f);
}

TEST(VolumetricFog, RainWetsTheLandAndAStormTakesTheGroundFogAway)
{
	float wetness = 0.0f;
	for (int i = 0; i < 600; ++i)
	{
		wetness = fog::StepWetness(wetness, 1.0f, 0.1f);
	}
	EXPECT_GT(wetness, 0.9f);
	// It dries over minutes, not seconds
	const float wet = wetness;
	for (int i = 0; i < 600; ++i)
	{
		wetness = fog::StepWetness(wetness, 0.0f, 0.1f);
	}
	EXPECT_GT(wetness, wet * 0.8f);
	// More fog on wet land; none in a storm, which thickens the haze instead
	EXPECT_GT(fog::FogFor(6.0f, 1.0f, 0.0f).groundDensity, fog::FogFor(6.0f, 0.0f, 0.0f).groundDensity * 2.0f);
	EXPECT_EQ(fog::FogFor(6.0f, 1.0f, 1.0f).groundDensity, 0.0f);
	EXPECT_GT(fog::FogFor(12.0f, 0.0f, 1.0f).hazeDensity, fog::FogFor(12.0f, 0.0f, 0.0f).hazeDensity * 3.0f);
}

TEST(VolumetricFog, APocketIsFullInsideAndSoftAtItsEdge)
{
	openblack::ecs::components::FogPocket box;
	box.shape = openblack::ecs::components::FogPocket::Shape::Box;
	box.centre = {100.0f, 10.0f, 200.0f};
	box.halfSize = {30.0f, 10.0f, 20.0f};
	box.density = 0.03f;
	box.edge = 4.0f;
	box.atDawn = true;
	EXPECT_NEAR(fog::PocketDensity(box, box.centre, 1.0f), 0.03f, 1e-6f);
	// Outside it there is none, and half way through the edge about half
	EXPECT_EQ(fog::PocketDensity(box, {100.0f, 10.0f, 230.0f}, 1.0f), 0.0f);
	EXPECT_NEAR(fog::PocketDensity(box, {128.0f, 10.0f, 200.0f}, 1.0f), 0.015f, 1e-4f);
	// A pocket of the dawn goes with the morning; one that is always there does not
	EXPECT_EQ(fog::PocketDensity(box, box.centre, 0.0f), 0.0f);
	box.atDawn = false;
	EXPECT_NEAR(fog::PocketDensity(box, box.centre, 0.0f), 0.03f, 1e-6f);
	openblack::ecs::components::FogPocket ball;
	ball.shape = openblack::ecs::components::FogPocket::Shape::Ball;
	ball.halfSize = glm::vec3(10.0f);
	ball.atDawn = false;
	EXPECT_GT(fog::PocketDensity(ball, {0.0f, 0.0f, 5.0f}, 0.0f), 0.0f);
	EXPECT_EQ(fog::PocketDensity(ball, {0.0f, 0.0f, 11.0f}, 0.0f), 0.0f);
}

TEST(VolumetricFog, TheNearestPocketsGoFirst)
{
	std::vector<openblack::ecs::components::FogPocket> pockets(10);
	for (size_t i = 0; i < pockets.size(); ++i)
	{
		pockets[i].centre = {static_cast<float>(100 * (10 - i)), 0.0f, 0.0f};
		pockets[i].atDawn = false;
		pockets[i].density = 0.01f;
	}
	const auto packed = fog::PackPockets(pockets, glm::vec3(0.0f), 1.0f);
	// The nearest (the last, at 100) is first, and only the eight nearest are kept
	EXPECT_EQ(packed.pockets[0].x, 100.0f);
	EXPECT_EQ(packed.pockets[fog::k_MostPockets - 1].x, 800.0f);
	EXPECT_NEAR(packed.pockets[fog::k_MostPockets].w, 0.01f, 1e-6f);
	EXPECT_GT(packed.edges[0].x, 0.0f);
	// No pockets: all densities are 0
	EXPECT_EQ(fog::PackPockets({}, glm::vec3(0.0f), 1.0f).pockets[fog::k_MostPockets].w, 0.0f);
}

TEST(VolumetricFog, StillWaterAndSwampGetALowBox)
{
	constexpr int k_Side = 32;
	std::vector<uint8_t> sounds(k_Side * k_Side, 7);
	std::vector<float> heights(k_Side * k_Side, 20.0f);
	// A swamp of 4 by 5 cells in the second block across, and two lone cells of still water in the first
	for (int z = 3; z < 8; ++z)
	{
		for (int x = 18; x < 22; ++x)
		{
			sounds[static_cast<size_t>(z * k_Side + x)] = 8;
			heights[static_cast<size_t>(z * k_Side + x)] = 12.0f;
		}
	}
	sounds[5] = 2;
	sounds[6] = 2;
	const auto pockets = fog::StillWaterPockets(sounds, heights, k_Side, 10.0f, 6);
	ASSERT_EQ(pockets.size(), 1u);
	const auto& box = pockets[0];
	EXPECT_NEAR(box.centre.x, (17.0f * 10.0f + 23.0f * 10.0f) * 0.5f, 1e-3f);
	EXPECT_NEAR(box.centre.z, (2.0f * 10.0f + 9.0f * 10.0f) * 0.5f, 1e-3f);
	EXPECT_NEAR(box.centre.y - box.halfSize.y, 10.0f, 1e-3f);
	EXPECT_NEAR(box.centre.y + box.halfSize.y, 20.0f, 1e-3f);
	EXPECT_TRUE(box.atDawn);
}
