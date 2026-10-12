/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <Input/PointerSpeed.h>
#include <gtest/gtest.h>

using openblack::input::pointer_speed::Scale;
using openblack::input::pointer_speed::ScaledPointer;

TEST(PointerSpeed, IsTheHeightOfTheScreensFourByThreeMiddleOverTheDefaultScreens)
{
	// The game's default screen moves the pointer as the system's cursor
	EXPECT_FLOAT_EQ(Scale({800, 600}), 1.0f);
	EXPECT_FLOAT_EQ(Scale({640, 480}), 0.8f);
	EXPECT_FLOAT_EQ(Scale({1600, 1200}), 2.0f);
	// Wider than 4:3 the view keeps the 4:3 height, so the height alone counts
	EXPECT_FLOAT_EQ(Scale({1920, 1080}), 1.8f);
	EXPECT_FLOAT_EQ(Scale({2560, 1440}), 2.4f);
	EXPECT_FLOAT_EQ(Scale({3440, 1440}), 2.4f);
	EXPECT_FLOAT_EQ(Scale({5120, 1440}), 2.4f);
	// Narrower than 4:3 the view keeps the 4:3 width: 1280x1024 moves as 1280x960
	EXPECT_FLOAT_EQ(Scale({1280, 1024}), 1.6f);
	// No screen leaves the pointer as it is
	EXPECT_FLOAT_EQ(Scale({0, 0}), 1.0f);
}

TEST(PointerSpeed, TheSameMouseMovementSweepsTheSameShareOfAnyScreen)
{
	// A movement across a quarter of the default screen's height
	const glm::ivec2 movement {40, -150};
	for (const glm::ivec2 screen : {glm::ivec2(800, 600), glm::ivec2(1280, 1024), glm::ivec2(1920, 1080),
	                                glm::ivec2(2560, 1440), glm::ivec2(3440, 1440), glm::ivec2(5120, 1440)})
	{
		const float scale = Scale(screen);
		ScaledPointer pointer;
		pointer.Place(screen / 2);
		const auto moved = pointer.Move(movement, scale, screen) - screen / 2;
		// A quarter of the height of the screen's 4:3 middle, in that middle's pixels
		EXPECT_NEAR(static_cast<float>(moved.y) / (600.0f * scale), -0.25f, 1.0f / 600.0f) << screen.x << "x" << screen.y;
		EXPECT_NEAR(static_cast<float>(moved.x) / (600.0f * scale), 40.0f / 600.0f, 1.0f / 600.0f)
		    << screen.x << "x" << screen.y;
	}
}

TEST(PointerSpeed, OnTheDefaultScreenThePointerMovesPixelForPixel)
{
	ScaledPointer pointer;
	pointer.Place({400, 300});
	EXPECT_EQ(pointer.Move({1, 0}, 1.0f, {800, 600}), glm::ivec2(401, 300));
	EXPECT_EQ(pointer.Move({-3, 7}, 1.0f, {800, 600}), glm::ivec2(398, 307));
}

TEST(PointerSpeed, SlowMovementsAddUp)
{
	// At 1.6 a pixel at a time moves the pointer 1.6 pixels on average
	ScaledPointer pointer;
	pointer.Place({100, 100});
	for (int i = 0; i < 10; ++i)
	{
		pointer.Move({1, 0}, 1.6f, {1280, 1024});
	}
	EXPECT_EQ(pointer.Position(), glm::ivec2(116, 100));
}

TEST(PointerSpeed, ThePointerStaysOnTheScreen)
{
	ScaledPointer pointer;
	pointer.Place({10, 10});
	EXPECT_EQ(pointer.Move({-100, -100}, 1.8f, {1920, 1080}), glm::ivec2(0, 0));
	EXPECT_EQ(pointer.Move({5000, 5000}, 1.8f, {1920, 1080}), glm::ivec2(1919, 1079));
	// And comes straight back off the edge
	EXPECT_EQ(pointer.Move({-1, -1}, 1.0f, {1920, 1080}), glm::ivec2(1918, 1078));
}
