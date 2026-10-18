/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the foam that builds up on the water, and of the silt in the water

#include <cmath>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Graphics/UnderwaterFoam.h"

namespace underwater = openblack::graphics::underwater;

TEST(UnderwaterFoam, TheFieldFollowsThePlaceTheViewMeetsTheWater)
{
	// An eye 100 over the water that looks down at 45 degrees meets the water 100 away
	const glm::vec3 eye {1000.0f, 100.0f, 2000.0f};
	const auto forward = glm::normalize(glm::vec3 {1.0f, -1.0f, 0.0f});
	const auto focus = underwater::FoamFocus(eye, forward, 0.0f);
	EXPECT_NEAR(focus.x, 1100.0f, 1e-3f);
	EXPECT_NEAR(focus.y, 2000.0f, 1e-3f);
	// A view that grazes the water goes no farther than the limit
	const auto far = underwater::FoamFocus(eye, glm::normalize(glm::vec3 {1.0f, -0.05f, 0.0f}), 0.0f);
	EXPECT_NEAR(glm::length(far - glm::vec2 {1000.0f, 2000.0f}), underwater::k_FoamFarthestFocus, 1e-3f);
	// A view that looks up, or an eye under the water, keeps the field under the eye
	EXPECT_EQ(underwater::FoamFocus(eye, glm::vec3 {0.0f, 1.0f, 0.0f}, 0.0f), glm::vec2(1000.0f, 2000.0f));
	EXPECT_EQ(underwater::FoamFocus({5.0f, -3.0f, 7.0f}, forward, 0.0f), glm::vec2(5.0f, 7.0f));
}

TEST(UnderwaterFoam, TheFieldLiesOnWholeTexels)
{
	const auto origin = underwater::FoamWindowOrigin({1000.3f, 2000.7f});
	EXPECT_NEAR(std::fmod(origin.x, underwater::k_FoamTexelSize), 0.0f, 1e-3f);
	EXPECT_NEAR(std::fmod(origin.y, underwater::k_FoamTexelSize), 0.0f, 1e-3f);
	// The middle stays inside one texel of the field's middle
	const auto middle = origin + glm::vec2(underwater::k_FoamWindow * 0.5f);
	EXPECT_LE(std::abs(middle.x - 1000.3f), underwater::k_FoamTexelSize);
	EXPECT_LE(std::abs(middle.y - 2000.7f), underwater::k_FoamTexelSize);
}

TEST(UnderwaterFoam, TheFieldMovesOnAtMostSixtyTimesASecond)
{
	underwater::FoamClock clock;
	EXPECT_FALSE(clock.Ready());
	const glm::vec2 focus {500.0f, 500.0f};
	// The first step starts the field with no foam
	const auto first = clock.Advance(10.0f, focus, 1);
	ASSERT_TRUE(first.has_value());
	EXPECT_TRUE(first->clear);
	EXPECT_TRUE(clock.Ready());
	EXPECT_EQ(clock.Latest(), first->target);
	// Frames at 200 a second: a step every fourth frame or so, never two in one sixtieth of a second
	int steps = 0;
	float covered = 0.0f;
	uint8_t target = first->target;
	for (int frame = 1; frame <= 200; ++frame)
	{
		if (const auto step = clock.Advance(10.0f + static_cast<float>(frame) * 0.005f, focus, 1); step.has_value())
		{
			EXPECT_FALSE(step->clear);
			EXPECT_NE(step->target, target);
			target = step->target;
			covered += step->seconds;
			++steps;
		}
	}
	EXPECT_LE(steps, 60);
	EXPECT_GE(steps, 45);
	EXPECT_NEAR(covered, 1.0f, 0.03f);
}

TEST(UnderwaterFoam, AStoppedGameKeepsItsFoam)
{
	underwater::FoamClock clock;
	ASSERT_TRUE(clock.Advance(5.0f, {}, 1).has_value());
	for (int frame = 0; frame < 10; ++frame)
	{
		EXPECT_FALSE(clock.Advance(5.0f, {}, 1).has_value());
	}
	// After a long pause, one step covers no more than the longest step
	const auto step = clock.Advance(9.0f, {}, 1);
	ASSERT_TRUE(step.has_value());
	EXPECT_FALSE(step->clear);
	EXPECT_FLOAT_EQ(step->seconds, underwater::k_FoamLongestStep);
}

TEST(UnderwaterFoam, ANewLandOrAJumpStartsTheFieldAgain)
{
	underwater::FoamClock clock;
	ASSERT_TRUE(clock.Advance(1.0f, {100.0f, 100.0f}, 1).has_value());
	// A small move keeps the foam and gives the old corner to the step
	const auto moved = clock.Advance(1.1f, {130.0f, 100.0f}, 1);
	ASSERT_TRUE(moved.has_value());
	EXPECT_FALSE(moved->clear);
	EXPECT_NE(moved->origin, moved->previousOrigin);
	// A jump of the camera clears it
	const auto jumped = clock.Advance(1.2f, {3000.0f, 100.0f}, 1);
	ASSERT_TRUE(jumped.has_value());
	EXPECT_TRUE(jumped->clear);
	// So does a new sea bed
	const auto relanded = clock.Advance(1.3f, {3000.0f, 100.0f}, 2);
	ASSERT_TRUE(relanded.has_value());
	EXPECT_TRUE(relanded->clear);
}

TEST(UnderwaterFoam, ShoreFoamStandsAtTwoThirdsOfItsFirstHeight)
{
	EXPECT_NEAR(underwater::FoamStandHeight(1.0f, 1.0f), 0.45f * 2.0f / 3.0f, 1e-6f);
	EXPECT_NEAR(underwater::FoamStandHeight(1.5f, 1.0f), 1.5f * 0.3f, 1e-6f);
	EXPECT_NEAR(underwater::FoamStandHeight(1.0f, 0.5f), 0.45f * 5.0f / 6.0f, 1e-6f);
	EXPECT_FLOAT_EQ(underwater::FoamStandHeight(0.0f, 1.0f), 0.0f);
	EXPECT_FLOAT_EQ(underwater::FoamStandHeight(-1.0f, 1.0f), 0.0f);
}

TEST(UnderwaterFoam, OtherFoamKeepsItsFullHeight)
{
	EXPECT_FLOAT_EQ(underwater::FoamStandHeight(1.0f, 0.0f), 0.45f);
	EXPECT_FLOAT_EQ(underwater::FoamStandHeight(2.0f, 0.0f), 0.9f);
}

TEST(UnderwaterSediment, StormsAndBigWavesStirMoreSilt)
{
	const float calm = underwater::SedimentStir(underwater::k_CalmSwash, 0.0f);
	EXPECT_NEAR(calm, 0.6f, 1e-5f);
	EXPECT_GT(underwater::SedimentStir(underwater::k_CalmSwash * 2.0f, 0.0f), calm);
	EXPECT_GT(underwater::SedimentStir(underwater::k_CalmSwash, 1.0f), calm);
	EXPECT_FLOAT_EQ(underwater::SedimentStir(underwater::k_CalmSwash * 10.0f, 1.0f), underwater::k_MostStir);
	// No waves stir nothing
	EXPECT_FLOAT_EQ(underwater::SedimentStir(0.0f, 1.0f), 0.0f);
}
