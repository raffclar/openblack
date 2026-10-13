/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/PickShapes.h"

namespace pick_shapes = openblack::ecs::pick_shapes;

TEST(PickShapes, ADispensersBallSitsHighOnItAndIsHalfAgainAsWide)
{
	// A building 10 m tall (half extents 3, 5 and 4), at twice its size, standing at (100, 20, 50)
	const auto ball = pick_shapes::DispenserBall({100.0f, 20.0f, 50.0f}, {3.0f, 5.0f, 4.0f}, 2.0f);
	// Eight tenths of its 20 m height above where it stands
	EXPECT_FLOAT_EQ(ball.centre.x, 100.0f);
	EXPECT_FLOAT_EQ(ball.centre.y, 20.0f + 16.0f);
	EXPECT_FLOAT_EQ(ball.centre.z, 50.0f);
	// Its width across the land is the larger half extent, scaled: 8 m, and the ball half again as wide
	EXPECT_FLOAT_EQ(ball.radius, 12.0f);
}

TEST(PickShapes, ABubblesBallIsAboutItsMiddleAtOnePointTwoTimesItsScale)
{
	const auto ball = pick_shapes::BubbleBall({1.0f, 2.0f, 3.0f}, 1.5f);
	EXPECT_EQ(ball.centre, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_FLOAT_EQ(ball.radius, 1.8f);
}

TEST(PickShapes, ABubbleIsPickedMovedTowardsTheCameraByItsWidth)
{
	// The camera 10 m away along x; the bubble 2 m across x and 3 m across z, at scale 1
	const auto offset = pick_shapes::BubblePickOffset({0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 3.0f}, 1.0f);
	EXPECT_FLOAT_EQ(offset.x, 3.0f);
	EXPECT_FLOAT_EQ(offset.y, 0.0f);
	EXPECT_FLOAT_EQ(offset.z, 0.0f);
	// A camera at its middle doesn't move it
	EXPECT_EQ(pick_shapes::BubblePickOffset({}, {}, {2.0f, 1.0f, 3.0f}, 1.0f), glm::vec3(0.0f));
}
