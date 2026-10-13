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
	const auto ball = pick_shapes::BubbleBall({1.0f, 2.0f, 3.0f}, 1.5f, glm::vec3(0.0f));
	EXPECT_EQ(ball.centre, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_FLOAT_EQ(ball.radius, 1.8f);
}

TEST(PickShapes, APicturesBallIsMovedByItsSeedModelsMiddleUnscaled)
{
	// A bubble at twice its size whose seed's model has its middle half a metre up: the ball moves by that half metre
	const auto bubble = pick_shapes::BubbleBall({1.0f, 2.0f, 3.0f}, 2.0f, {0.0f, 0.5f, 0.0f});
	EXPECT_EQ(bubble.centre, glm::vec3(1.0f, 2.5f, 3.0f));
	EXPECT_FLOAT_EQ(bubble.radius, 2.4f);
	// An icon's picture keeps its size of 1: a ball of 2 m
	const auto icon = pick_shapes::PictureBall({10.0f, 5.0f, 0.0f}, pick_shapes::k_IconPictureSize, {0.25f, 0.0f, 0.0f});
	EXPECT_EQ(icon.centre, glm::vec3(10.25f, 5.0f, 0.0f));
	EXPECT_FLOAT_EQ(icon.radius, 2.0f);
}

TEST(PickShapes, AVortexsBallIsSevenTenthsOfItsCylindersWidth)
{
	const auto ball = pick_shapes::VortexBall({5.0f, 6.0f, 7.0f}, {10.0f, 30.0f, 20.0f});
	EXPECT_EQ(ball.centre, glm::vec3(5.0f, 6.0f, 7.0f));
	EXPECT_FLOAT_EQ(ball.radius, 14.0f);
}

TEST(PickShapes, ATeleportStonesAndALeashsBallsHaveFixedSizes)
{
	EXPECT_FLOAT_EQ(pick_shapes::TeleportBall({1.0f, 2.0f, 3.0f}).radius, 3.0f);
	EXPECT_EQ(pick_shapes::TeleportBall({1.0f, 2.0f, 3.0f}).centre, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_FLOAT_EQ(pick_shapes::LeashPostBall({4.0f, 5.0f, 6.0f}).radius, 1.0f);
	EXPECT_EQ(pick_shapes::LeashPostBall({4.0f, 5.0f, 6.0f}).centre, glm::vec3(4.0f, 5.0f, 6.0f));
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
