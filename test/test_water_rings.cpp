/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <chrono>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/WaterRings.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/WaterRingSystem.h"

using namespace openblack;

TEST(WaterRings, TheHandsSplash)
{
	const auto ring = water_rings::HandSplash({10.0f, 20.0f}, 1.5f, 0x123456u);
	EXPECT_EQ(ring.position, glm::vec3(10.0f, 0.2f, 20.0f));
	EXPECT_FLOAT_EQ(ring.growth, 7.0f);
	EXPECT_FLOAT_EQ(ring.angle, 1.5f);
	EXPECT_EQ(ring.cell, 0x30);
	EXPECT_EQ(ring.argb, 0xB0123456u);
	EXPECT_EQ(ring.age, 0u);
}

TEST(WaterRings, GrowsAndFadesOverItsLife)
{
	water_rings::Ring ring {.growth = 7.0f, .argb = 0xFFFFFFFFu};
	EXPECT_FLOAT_EQ(water_rings::HalfWidth(ring), 0.0001f);
	EXPECT_EQ(water_rings::Alpha(ring), 254);
	ASSERT_TRUE(water_rings::Advance(ring, 350.0f));
	EXPECT_EQ(ring.age, 350u);
	EXPECT_FLOAT_EQ(water_rings::HalfWidth(ring), 3.5f);
	EXPECT_EQ(water_rings::Alpha(ring), 127);
	EXPECT_FALSE(water_rings::Advance(ring, 350.0f));
}

TEST(WaterRings, AgesAtItsRateInWholeMilliseconds)
{
	water_rings::Ring ring {.rate = 0.5f};
	ASSERT_TRUE(water_rings::Advance(ring, 33.0f));
	EXPECT_EQ(ring.age, 16u);
}

TEST(WaterRings, AFlatSquareTurnedAboutItsMiddle)
{
	water_rings::Ring ring {.position = {5.0f, 0.2f, 5.0f}, .age = 700, .growth = 1.0f, .angle = 0.7f, .aspect = 2.0f};
	const auto corners = water_rings::Corners(ring);
	for (const auto& corner : corners)
	{
		EXPECT_FLOAT_EQ(corner.y, 0.2f);
	}
	// Twice as deep as wide
	EXPECT_NEAR(glm::distance(corners[0], corners[1]), 2.0f, 1e-5f);
	EXPECT_NEAR(glm::distance(corners[1], corners[2]), 4.0f, 1e-5f);
	EXPECT_NEAR(glm::distance((corners[0] + corners[2]) * 0.5f, ring.position), 0.0f, 1e-5f);
}

TEST(WaterRings, ItsWidthLiesAlongItsAngle)
{
	// Turned a quarter from +x towards +z, half as deep as wide
	water_rings::Ring ring {.age = 700, .growth = 4.0f, .angle = 1.5707964f, .aspect = 0.5f};
	const auto corners = water_rings::Corners(ring);
	// From the first corner to the second runs along +z, its width; from the second to the third runs across it, -x
	EXPECT_NEAR(corners[1].z - corners[0].z, 8.0f, 1e-5f);
	EXPECT_NEAR(corners[1].x - corners[0].x, 0.0f, 1e-5f);
	EXPECT_NEAR(corners[2].x - corners[1].x, -4.0f, 1e-5f);
	EXPECT_NEAR(corners[2].z - corners[1].z, 0.0f, 1e-5f);
}

TEST(WaterRings, CellsOfTheSmokeTexture)
{
	const auto uvs = water_rings::CellUvs(0x30);
	EXPECT_EQ(uvs[0], glm::vec2(0.0f, 0.75f));
	EXPECT_EQ(uvs[2], glm::vec2(0.125f, 0.875f));
	// Only the low six bits pick the cell
	EXPECT_EQ(water_rings::CellUvs(0x7F)[0], water_rings::CellUvs(0x3F)[0]);
}

TEST(WaterRings, StepAtTheScreensRefreshWhateverTheFrameRate)
{
	using std::chrono::milliseconds;
	// 1000 frames a second: a step every 16th or 17th frame, with all the game time since the last
	water_rings::Pace pace;
	int32_t steps = 0;
	milliseconds aged {0};
	for (int32_t frame = 0; frame < 1000; ++frame)
	{
		const auto step = water_rings::Step(pace, milliseconds(1), milliseconds(1));
		if (step.count() != 0)
		{
			++steps;
			aged += step;
			EXPECT_TRUE(step == milliseconds(16) || step == milliseconds(17));
		}
	}
	EXPECT_EQ(steps, 60);
	EXPECT_EQ(aged + pace.gameSinceStep, milliseconds(1000));
}

TEST(WaterRings, SlowFramesStepOnceEachWithTheirGameTime)
{
	using std::chrono::milliseconds;
	water_rings::Pace pace;
	EXPECT_EQ(water_rings::Step(pace, milliseconds(33), milliseconds(33)), milliseconds(33));
	EXPECT_EQ(water_rings::Step(pace, milliseconds(40), milliseconds(80)), milliseconds(80));
	// A long frame steps once and leaves no steps owed, only part of an interval towards the next
	EXPECT_EQ(water_rings::Step(pace, milliseconds(500), milliseconds(500)), milliseconds(500));
	EXPECT_LT(pace.sinceStep, water_rings::Pace::k_StepInterval);
	EXPECT_EQ(water_rings::Step(pace, milliseconds(1), milliseconds(1)), milliseconds(0));
	// The next step takes the game time owed; while the game is paused the frames bring none
	EXPECT_EQ(water_rings::Step(pace, milliseconds(50), milliseconds(0)), milliseconds(1));
	EXPECT_EQ(water_rings::Step(pace, milliseconds(50), milliseconds(0)), milliseconds(0));
}

TEST(WaterRingSystem, ABigBodysRingGrowsAtHighFrameRates)
{
	using std::chrono::milliseconds;
	// A body of radius 2.23 makes a ring aging at 1/2.23: every frame on its own, 2 ms frames would never age it
	ecs::systems::WaterRingSystem rings;
	ASSERT_TRUE(rings.Add({.growth = 4.46f, .rate = 1.0f / 2.23f}));
	for (int32_t frame = 0; frame < 250; ++frame)
	{
		rings.Update(milliseconds(2), milliseconds(2));
	}
	// Half a second: 29 steps of 17 or 18 ms, each 7 or 8 ms of the ring's age, as at 60 frames a second
	ASSERT_EQ(rings.GetRings().size(), 1u);
	EXPECT_GT(rings.GetRings()[0].age, 200u);
	EXPECT_LT(rings.GetRings()[0].age, 230u);
}

TEST(WaterRingSystem, HoldsAtMostItsRingsAndLetsThemGo)
{
	using std::chrono::milliseconds;
	ecs::systems::WaterRingSystem rings;
	for (size_t i = 0; i < water_rings::k_MostRings; ++i)
	{
		ASSERT_TRUE(rings.Add({}));
	}
	EXPECT_FALSE(rings.Add({}));
	rings.Update(milliseconds(699), milliseconds(699));
	EXPECT_EQ(rings.GetRings().size(), water_rings::k_MostRings);
	rings.Update(milliseconds(20), milliseconds(1));
	EXPECT_TRUE(rings.GetRings().empty());
	ASSERT_TRUE(rings.Add({}));
	rings.Reset();
	EXPECT_TRUE(rings.GetRings().empty());
}
