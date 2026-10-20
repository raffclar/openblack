/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <numbers>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "Gui/WorldTextDepth.h"

using namespace openblack::gui::world_text;

// A camera that looks level puts the label 5 nearer than its point
TEST(WorldTextDepth, LevelCameraPullsByFive)
{
	EXPECT_FLOAT_EQ(DrawnDepth(100.0f, 0.0f, 1.0f), 95.0f);
}

// A camera that looks straight down puts the label at two times the near plane, in front of the world
TEST(WorldTextDepth, StraightDownIsTwiceTheNearPlane)
{
	EXPECT_FLOAT_EQ(DrawnDepth(100.0f, std::numbers::pi_v<float> / 2.0f, 1.0f), 2.0f);
	EXPECT_FLOAT_EQ(DrawnDepth(100.0f, 3.0f, 1.0f), 2.0f);
}

// The sign of the pitch does not change the depth
TEST(WorldTextDepth, PitchSignDoesNotMatter)
{
	EXPECT_EQ(DrawnDepth(80.0f, -0.7f, 1.0f), DrawnDepth(80.0f, 0.7f, 1.0f));
}

// Half way down, the pull is three quarters of the depth past two near planes
TEST(WorldTextDepth, HalfWayDown)
{
	const float drawn = DrawnDepth(102.0f, std::numbers::pi_v<float> / 4.0f, 1.0f);
	EXPECT_NEAR(drawn, 2.0f + (95.0f * 0.75f), 0.01f);
}

// A point nearer than two near planes is drawn at two near planes
TEST(WorldTextDepth, NeverNearerThanTwoNearPlanes)
{
	EXPECT_FLOAT_EQ(DrawnDepth(3.0f, 0.0f, 1.0f), 2.0f);
	EXPECT_FLOAT_EQ(DrawnDepth(0.0f, 0.2f, 0.5f), 1.0f);
}

// The value in the depth buffer of a depth comes from the projection, here one that puts the near plane at 1 and the
// distance at 0
TEST(WorldTextDepth, BufferDepthFollowsTheProjection)
{
	const glm::mat4 reverse(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 1.0f);
	const auto projection = reverse * glm::infinitePerspectiveLH(1.0f, 1.0f, 2.0f);
	EXPECT_NEAR(BufferDepth(projection, 2.0f), 1.0f, 1e-6f);
	EXPECT_NEAR(BufferDepth(projection, 8.0f), 0.25f, 1e-6f);
}
