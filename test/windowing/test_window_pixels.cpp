/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Windowing/WindowPixels.h"

using namespace openblack::windowing;

TEST(WindowPixels, TheScaleIsThePixelsForEachWindowUnit)
{
	EXPECT_EQ(PixelScale({1920, 1080}, {1920, 1080}), glm::vec2(1.0f));
	EXPECT_EQ(PixelScale({1920, 1080}, {3840, 2160}), glm::vec2(2.0f));
	EXPECT_EQ(PixelScale({1280, 720}, {1600, 900}), glm::vec2(1.25f));
}

TEST(WindowPixels, AWindowOfNoSizeHasAScaleOfOne)
{
	EXPECT_EQ(PixelScale({0, 0}, {0, 0}), glm::vec2(1.0f));
	EXPECT_EQ(PixelScale({1280, 0}, {2560, 0}), glm::vec2(1.0f));
	EXPECT_EQ(PixelScale({1280, 720}, {0, 0}), glm::vec2(1.0f));
}

TEST(WindowPixels, APointDoesNotChangeAtAScaleOfOne)
{
	EXPECT_EQ(PointToPixels({640, 330}, glm::vec2(1.0f)), glm::ivec2(640, 330));
	EXPECT_EQ(PointToPixels({-3, 1023}, glm::vec2(1.0f)), glm::ivec2(-3, 1023));
}

TEST(WindowPixels, APointGoesToThePixelAtTheTopLeftOfItsUnit)
{
	EXPECT_EQ(PointToPixels({640, 330}, glm::vec2(2.0f)), glm::ivec2(1280, 660));
	EXPECT_EQ(PointToPixels({1, 3}, glm::vec2(1.5f)), glm::ivec2(1, 4));
	// A point to the left of the window stays to the left of it
	EXPECT_EQ(PointToPixels({-1, 0}, glm::vec2(1.5f)), glm::ivec2(-2, 0));
}

TEST(WindowPixels, AMovementDoesNotChangeAtAScaleOfOne)
{
	MovementToPixels pixels;
	EXPECT_EQ(pixels.Move({5, -7}, glm::vec2(1.0f)), glm::ivec2(5, -7));
	EXPECT_EQ(pixels.Move({1, 1}, glm::vec2(1.0f)), glm::ivec2(1, 1));
}

TEST(WindowPixels, AMovementGrowsByTheScale)
{
	MovementToPixels pixels;
	EXPECT_EQ(pixels.Move({-20, 3}, glm::vec2(2.0f)), glm::ivec2(-40, 6));
}

TEST(WindowPixels, SlowMovementsAddUp)
{
	MovementToPixels pixels;
	// At 1.5 each movement of one unit gives 1, then 2 pixels: 3 pixels for each 2 units
	EXPECT_EQ(pixels.Move({1, -1}, glm::vec2(1.5f)), glm::ivec2(1, -1));
	EXPECT_EQ(pixels.Move({1, -1}, glm::vec2(1.5f)), glm::ivec2(2, -2));
	EXPECT_EQ(pixels.Move({1, -1}, glm::vec2(1.5f)), glm::ivec2(1, -1));
	EXPECT_EQ(pixels.Move({1, -1}, glm::vec2(1.5f)), glm::ivec2(2, -2));
}

TEST(WindowPixels, AMovementBackCancelsThePartThatStays)
{
	MovementToPixels pixels;
	EXPECT_EQ(pixels.Move({1, 0}, glm::vec2(1.5f)), glm::ivec2(1, 0));
	// Half a pixel stays; a movement back of one unit gives -1.5 + 0.5 = -1
	EXPECT_EQ(pixels.Move({-1, 0}, glm::vec2(1.5f)), glm::ivec2(-1, 0));
	EXPECT_EQ(pixels.Move({0, 0}, glm::vec2(1.5f)), glm::ivec2(0, 0));
}
