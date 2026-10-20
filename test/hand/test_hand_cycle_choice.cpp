/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "3D/HandCycleChoice.h"

using namespace openblack;
using namespace openblack::hand_cycle;
using Cycle = HandAnimation::Cycle;
using Pose = hand_navigation_pose::Pose;
using Rope = leash_snap::Hand;

TEST(HandCycleChoice, ThePoseThatMovesTheCameraGivesItsCycle)
{
	EXPECT_EQ(Choose({}), Cycle::Wiggle);
	EXPECT_EQ(Choose({.pose = Pose::Grip}), Cycle::Grip);
	EXPECT_EQ(Choose({.pose = Pose::Rotate}), Cycle::Rotate);
	EXPECT_EQ(Choose({.pose = Pose::Pitch}), Cycle::Pitch);
	EXPECT_EQ(Choose({.pose = Pose::Zoom}), Cycle::Zoom);
}

TEST(HandCycleChoice, HoveringOverAThingTheHandFeelsItOverTheCameraHints)
{
	EXPECT_EQ(Choose({.overThing = true}), Cycle::Stroke);
	EXPECT_EQ(Choose({.pose = Pose::Rotate, .overThing = true}), Cycle::Stroke);
	// The camera state keeps its own cycle
	EXPECT_EQ(Choose({.pose = Pose::Grip, .cameraState = true, .overThing = true}), Cycle::Grip);
}

TEST(HandCycleChoice, OverAMiraclesIconTheFingersOpen)
{
	EXPECT_EQ(Choose({.overSpellIcon = true}), Cycle::CanPickUp);
	EXPECT_EQ(Choose({.overThing = true, .overSpellIcon = true}), Cycle::CanPickUp);
	EXPECT_EQ(Choose({.cameraState = true, .overSpellIcon = true}), Cycle::Wiggle);
}

TEST(HandCycleChoice, TheRopeOfTheLeashGivesThePinch)
{
	EXPECT_EQ(Choose({.rope = Rope::Over}), Cycle::HoldFingers);
	EXPECT_EQ(Choose({.rope = Rope::Holding}), Cycle::HoldFingers);
	EXPECT_EQ(Choose({.overThing = true, .rope = Rope::Over}), Cycle::HoldFingers);
	// A hand that holds the rope over a thing or an icon feels the thing or opens for the icon
	EXPECT_EQ(Choose({.overThing = true, .rope = Rope::Holding}), Cycle::Stroke);
	EXPECT_EQ(Choose({.overSpellIcon = true, .rope = Rope::Holding}), Cycle::CanPickUp);
	EXPECT_EQ(Choose({.cameraState = true, .rope = Rope::Over}), Cycle::Wiggle);
}

TEST(HandCycleChoice, ACreatureInTheHandGivesItsTouch)
{
	using Touch = creature_feedback::TouchCycle;
	EXPECT_EQ(Choose({.creature = Touch::Wiggle}), Cycle::Wiggle);
	EXPECT_EQ(Choose({.creature = Touch::Tickle}), Cycle::Tickle);
	EXPECT_EQ(Choose({.overThing = true, .rope = Rope::Over, .creature = Touch::Slap}), Cycle::Slap);
}
