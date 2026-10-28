/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Debug/TestbedHandSweep.h"

using namespace openblack::testbed;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(TestbedHandSweep, ARayDownMeetsThePlaneUnderIt)
{
	const auto at = hand_sweep::OnPlane({10.0f, 50.0f, 20.0f}, {0.5f, -1.0f, -0.25f}, 10.0f);
	ASSERT_TRUE(at.has_value());
	EXPECT_EQ(*at, glm::vec2(30.0f, 10.0f));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(TestbedHandSweep, ARayThatGoesAwayOrAlongThePlaneMeetsNothing)
{
	EXPECT_FALSE(hand_sweep::OnPlane({0.0f, 50.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 10.0f).has_value());
	EXPECT_FALSE(hand_sweep::OnPlane({0.0f, 50.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 10.0f).has_value());
}
