/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The mouse buttons and motion the hand reads (Input/MouseButtons), with fake button events: what Game::ProcessEvents
// and Game::Update did with their function statics

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Input/MouseButtons.h"

using namespace openblack::input;

namespace
{
MouseButtonEvent Down(MouseButton button, glm::ivec2 position = {0, 0})
{
	return {.button = button, .down = true, .position = position};
}

MouseButtonEvent Up(MouseButton button, glm::ivec2 position = {0, 0})
{
	return {.button = button, .down = false, .position = position};
}
} // namespace

TEST(MouseButtons, startsWithNothingHeld)
{
	const MouseButtonsState state;
	EXPECT_FALSE(HandGripping(state));
	EXPECT_FALSE(HandAction(state));
	EXPECT_EQ(state.middlePressPosition, glm::ivec2(0, 0));
}

TEST(MouseButtons, leftPressAndReleaseGripsThenLetsGo)
{
	MouseButtonsState state;
	ApplyMouseButton(state, Down(MouseButton::Left));
	EXPECT_TRUE(state.left);
	EXPECT_TRUE(HandGripping(state));
	ApplyMouseButton(state, Up(MouseButton::Left));
	EXPECT_FALSE(state.left);
	EXPECT_FALSE(HandGripping(state));
}

TEST(MouseButtons, leftFlipsOnEveryEventNotOnItsDirection)
{
	// two downs in a row (an up lost while the window had no focus) leave the left button let go
	MouseButtonsState state;
	ApplyMouseButton(state, Down(MouseButton::Left));
	ApplyMouseButton(state, Down(MouseButton::Left));
	EXPECT_FALSE(state.left);
	// an up first holds it
	ApplyMouseButton(state, Up(MouseButton::Left));
	EXPECT_TRUE(state.left);
}

TEST(MouseButtons, middleFlipsAndKeepsWhereItWentDown)
{
	MouseButtonsState state;
	ApplyMouseButton(state, Down(MouseButton::Middle, {120, 340}));
	EXPECT_TRUE(state.middle);
	EXPECT_TRUE(HandGripping(state));
	EXPECT_EQ(state.middlePressPosition, glm::ivec2(120, 340));
	// the release does not move the kept position
	ApplyMouseButton(state, Up(MouseButton::Middle, {500, 20}));
	EXPECT_FALSE(state.middle);
	EXPECT_EQ(state.middlePressPosition, glm::ivec2(120, 340));
	// a double press: the second down flips it back off and keeps its own position
	ApplyMouseButton(state, Down(MouseButton::Middle, {1, 2}));
	ApplyMouseButton(state, Down(MouseButton::Middle, {3, 4}));
	EXPECT_FALSE(state.middle);
	EXPECT_EQ(state.middlePressPosition, glm::ivec2(3, 4));
}

TEST(MouseButtons, rightIsHeldWhileDown)
{
	MouseButtonsState state;
	ApplyMouseButton(state, Down(MouseButton::Right));
	EXPECT_TRUE(HandAction(state));
	ApplyMouseButton(state, Down(MouseButton::Right));
	EXPECT_TRUE(HandAction(state));
	ApplyMouseButton(state, Up(MouseButton::Right));
	EXPECT_FALSE(HandAction(state));
	ApplyMouseButton(state, Up(MouseButton::Right));
	EXPECT_FALSE(HandAction(state));
	EXPECT_FALSE(HandGripping(state));
}

TEST(MouseButtons, gripIsLeftOrMiddle)
{
	MouseButtonsState state;
	ApplyMouseButton(state, Down(MouseButton::Left));
	ApplyMouseButton(state, Down(MouseButton::Middle));
	ApplyMouseButton(state, Up(MouseButton::Left));
	EXPECT_TRUE(HandGripping(state));
	ApplyMouseButton(state, Up(MouseButton::Middle));
	EXPECT_FALSE(HandGripping(state));
}

TEST(MouseButtons, otherButtonsChangeNothing)
{
	MouseButtonsState state;
	ApplyMouseButton(state, Down(MouseButton::Middle, {7, 8}));
	ApplyMouseButton(state, Down(MouseButton::Other, {9, 9}));
	ApplyMouseButton(state, Up(MouseButton::Other, {9, 9}));
	EXPECT_FALSE(state.left);
	EXPECT_TRUE(state.middle);
	EXPECT_FALSE(state.right);
	EXPECT_EQ(state.middlePressPosition, glm::ivec2(7, 8));
}

TEST(MouseMotion, theFirstFrameHasNoMotion)
{
	MouseMotionState state;
	EXPECT_EQ(MouseMotion(state, {400, 300}), glm::ivec2(0, 0));
}

TEST(MouseMotion, eachFrameIsTheMoveSinceTheLast)
{
	MouseMotionState state;
	(void)MouseMotion(state, {400, 300});
	EXPECT_EQ(MouseMotion(state, {410, 295}), glm::ivec2(10, -5));
	EXPECT_EQ(MouseMotion(state, {410, 295}), glm::ivec2(0, 0));
	EXPECT_EQ(MouseMotion(state, {0, 0}), glm::ivec2(-410, -295));
}
