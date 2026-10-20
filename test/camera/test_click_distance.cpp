/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/ClickDistance.h"

using namespace openblack::click_distance;

TEST(ClickDistance, TheRaysGoDownTheMiddleColumn)
{
	EXPECT_EQ(RayCoord(0), glm::vec2(0.5f, 0.0f));
	EXPECT_EQ(RayCoord(15), glm::vec2(0.5f, 0.9375f));
	// At the game's 640 by 480 screen, ray 1 goes through the pixel (320, 30)
	EXPECT_EQ(RayCoord(1) * glm::vec2(640.0f, 480.0f), glm::vec2(320.0f, 30.0f));
}

TEST(ClickDistance, WithNoHitTheDistanceIsFifty)
{
	EXPECT_EQ(MeanDistance({}), k_NoHitDistance);
	EXPECT_EQ(std::bit_cast<uint32_t>(1.0f / k_NoHitDistance), 0x3CA3D70Au);
}

TEST(ClickDistance, TheMeanIsHarmonicWithOneMoreHitAtFifty)
{
	const std::array<float, 1> one {25.0f};
	// 1 / ((1/25 + 1/50) / 2)
	EXPECT_NEAR(MeanDistance(one), 33.3333f, 1e-3f);
	const std::array<float, 3> three {100.0f, 100.0f, 100.0f};
	EXPECT_NEAR(MeanDistance(three), 80.0f, 1e-3f);
}

TEST(ClickDistance, TheHitDistanceAddsTheSquaresZYX)
{
	EXPECT_EQ(HitDistance({0.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 4.0f}), 5.0f);
}

TEST(ClickDistance, ALevelOrUpwardCameraKeepsTheMeanAndASteepOneMovesTowardsTheFocusDistance)
{
	// Level: no share
	EXPECT_EQ(TowardsFocus(60.0f, 100.0f, {0.0f, 0.0f, -10.0f}, {0.0f, 0.0f, 0.0f}), 60.0f);
	// Looking up: no share
	EXPECT_EQ(TowardsFocus(60.0f, 100.0f, {0.0f, -5.0f, -10.0f}, {0.0f, 0.0f, 0.0f}), 60.0f);
	// Looking down at 45 degrees: the share is the pitch times the constant
	const float share = 0.7853982f * k_PitchShare;
	EXPECT_NEAR(TowardsFocus(60.0f, 100.0f, {0.0f, 10.0f, -10.0f}, {0.0f, 0.0f, 0.0f}), 60.0f + (40.0f * share), 1e-3f);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_PitchShare), 0x3D594CAFu);
}
