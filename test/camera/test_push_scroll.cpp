/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <chrono>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/PushScroll.h"

using namespace openblack;
namespace push = openblack::camera_push_scroll;

TEST(PushScroll, OnTheDefaultScreenThePointerIsItsOwn)
{
	for (const glm::ivec2 position : {glm::ivec2 {0, 0}, glm::ivec2 {799, 599}, glm::ivec2 {123, 456}})
	{
		EXPECT_EQ(push::PushScreenPosition(position, {800, 600}), position);
	}
}

TEST(PushScroll, OtherScreensAreLaidOverTheDefaultOne)
{
	// A bigger 4:3 screen: its sides are the default screen's sides
	EXPECT_EQ(push::PushScreenPosition({1599, 1199}, {1600, 1200}), glm::ivec2(799, 599));
	EXPECT_EQ(push::PushScreenPosition({0, 0}, {1600, 1200}), glm::ivec2(0, 0));
	// A wide screen folds its middle strip away: its sides are the 4:3 area's sides, its middle the area's middle
	EXPECT_EQ(push::PushScreenPosition({1919, 1079}, {1920, 1080}), glm::ivec2(799, 599));
	EXPECT_EQ(push::PushScreenPosition({0, 540}, {1920, 1080}), glm::ivec2(0, 300));
	EXPECT_EQ(push::PushScreenPosition({960, 540}, {1920, 1080}), glm::ivec2(400, 300));
	EXPECT_EQ(push::PushScreenPosition({5119, 0}, {5120, 1440}), glm::ivec2(799, 0));
}

// At every screen shape the push tests, the sides are the default screen's sides and the middle its middle
TEST(PushScroll, EveryScreenShapeHasTheDefaultScreensSidesAndMiddle)
{
	for (const glm::ivec2 size : {glm::ivec2 {1280, 1024}, glm::ivec2 {1920, 1080}, glm::ivec2 {2560, 1440},
	                              glm::ivec2 {3440, 1440}, glm::ivec2 {5120, 1440}})
	{
		EXPECT_EQ(push::PushScreenPosition({0, 0}, size), glm::ivec2(0, 0)) << size.x;
		EXPECT_EQ(push::PushScreenPosition(size - 1, size), glm::ivec2(799, 599)) << size.x;
		EXPECT_EQ(push::PushScreenPosition(size / 2, size), glm::ivec2(400, 300)) << size.x;
	}
}

TEST(PushScroll, ThePointerIsInsideAwayFromTheFirstAndLastRowsAndColumns)
{
	EXPECT_EQ(push::Inside({1, 1}, {1920, 1080}), glm::bvec2(true, true));
	EXPECT_EQ(push::Inside({0, 1078}, {1920, 1080}), glm::bvec2(false, true));
	EXPECT_EQ(push::Inside({1919, 1079}, {1920, 1080}), glm::bvec2(false, false));
}

TEST(PushScroll, BeyondASideThePointerAddsUpTheMouseMovement)
{
	push::Pointer pointer;
	push::Track(pointer, {5, 300}, {-3, 0}, {true, true}, false);
	EXPECT_EQ(pointer.beyond, glm::ivec2(5, 300));
	push::Track(pointer, {0, 300}, {-10, 0}, {false, true}, false);
	EXPECT_EQ(pointer.beyond, glm::ivec2(-5, 300));
	push::Track(pointer, {0, 300}, {-200, 4}, {false, true}, false);
	EXPECT_EQ(pointer.beyond, glm::ivec2(-205, 300));
	EXPECT_FALSE(pointer.insideX);
	// Back inside, the pointer is its own again
	push::Track(pointer, {7, 300}, {7, 0}, {true, true}, false);
	EXPECT_EQ(pointer.beyond, glm::ivec2(7, 300));
	// In a window the push never reads beyond the sides
	push::Track(pointer, {0, 300}, {-50, 0}, {false, true}, true);
	EXPECT_EQ(pointer.beyond, glm::ivec2(0, 300));
	EXPECT_TRUE(pointer.insideX);
}

TEST(PushScroll, TheSpeedGrowsWithTheHeightBetweenTheCameraAndItsFocus)
{
	EXPECT_EQ(push::Speed(10.0f, 0.0f), 30.0f);     // at least 60, halved
	EXPECT_EQ(push::Speed(100.0f, 0.0f), 150.0f);   // 300, halved
	EXPECT_EQ(push::Speed(1000.0f, 0.0f), 1000.0f); // at most 2000, halved
	EXPECT_EQ(push::Speed(0.0f, 1000.0f), 120.0f);  // below what it looks at, at most 240
}

TEST(PushScroll, OnlyAPushFurtherOutPastAnEighthOfTheWidthAddsSpeed)
{
	push::Pointer pointer {.beyond = {-98, 300}, .position = {0, 300}, .delta = {-5, 0}};
	glm::vec3 speed {0.0f};
	push::Push(speed, pointer, 100.0f, 0.01f);
	EXPECT_EQ(speed, glm::vec3(0.0f));
	pointer.beyond.x = -99;
	push::Push(speed, pointer, 100.0f, 0.01f);
	EXPECT_LT(speed.x, 0.0f);
	// Moving back in adds nothing
	speed = glm::vec3(0.0f);
	pointer.delta.x = 5;
	push::Push(speed, pointer, 100.0f, 0.01f);
	EXPECT_EQ(speed, glm::vec3(0.0f));
}

TEST(PushScroll, WithoutTheFeatureTheSpeedStopsAndNothingMoves)
{
	glm::vec3 speed {100.0f, 50.0f, 20.0f};
	const auto strafe = push::StrafeStep(speed, 0.01f, 0.0f, 0.5f, 0x0BF);
	EXPECT_EQ(speed, glm::vec3(0.0f));
	EXPECT_EQ(strafe.move, glm::vec3(0.0f));
	EXPECT_EQ(strafe.turn, 0.0f);
}

TEST(PushScroll, TheSpeedFadesFasterInsideAndStopsWhenNearlyStill)
{
	glm::vec3 inside {100.0f, 100.0f, 100.0f};
	glm::vec3 atSide = inside;
	push::Fade(inside, 0.1f, true);
	push::Fade(atSide, 0.1f, false);
	EXPECT_LT(inside.x, atSide.x);
	EXPECT_LT(atSide.z, atSide.x);
	glm::vec3 slow {0.05f, 0.05f, 0.0f};
	push::Fade(slow, 0.001f, false);
	EXPECT_EQ(slow, glm::vec3(0.0f));
}

TEST(PushScroll, TheClockGivesWholeMillisecondsAndKeepsTheRest)
{
	push::MillisecondClock clock;
	EXPECT_EQ(clock.Step(std::chrono::microseconds(10'600)), 10u);
	EXPECT_EQ(clock.Step(std::chrono::microseconds(10'600)), 11u);
	EXPECT_EQ(clock.Step(std::chrono::microseconds(-5)), 0u);
	EXPECT_FLOAT_EQ(push::Seconds(16), 0.016f);
}
