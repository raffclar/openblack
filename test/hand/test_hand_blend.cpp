/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Hand/HandBlend.h"

using namespace openblack;
using namespace openblack::hand_blend;
using Cycle = HandAnimation::Cycle;

namespace
{
Frame InMode(HandMode mode, Cycle cycle = Cycle::Wiggle)
{
	return {.mode = mode, .cycle = cycle};
}
} // namespace

TEST(HandBlend, TheModeOfTheHandFollowsWhatItHoldsBeforeWhatItTouches)
{
	EXPECT_EQ(ModeOf({}), HandMode::Normal);
	EXPECT_EQ(ModeOf({.shown = false, .holds = true}), HandMode::Hidden);
	EXPECT_EQ(ModeOf({.insideTemple = true, .holds = true, .movesCamera = true}), HandMode::Temple);
	EXPECT_EQ(ModeOf({.holds = true, .holdsSeed = true, .movesCamera = true}), HandMode::Seed);
	EXPECT_EQ(ModeOf({.pulls = true, .knocks = true}), HandMode::Pulling);
	EXPECT_EQ(ModeOf({.holds = true, .gripsTotem = true}), HandMode::Holding);
	EXPECT_EQ(ModeOf({.knocks = true, .movesCamera = true}), HandMode::Knocking);
	EXPECT_EQ(ModeOf({.gripsTotem = true}), HandMode::Totem);
	EXPECT_EQ(ModeOf({.touchesCreature = true, .movesCamera = true}), HandMode::Creature);
	EXPECT_EQ(ModeOf({.movesCamera = true}), HandMode::Camera);
}

TEST(HandBlend, EachChangeOfModeStartsABlendButTheFirstFrameDoesNot)
{
	ChangeTracker tracker;
	EXPECT_EQ(tracker.Step(InMode(HandMode::Normal)).count, 0);
	EXPECT_EQ(tracker.Step(InMode(HandMode::Normal)).count, 0);
	for (const auto mode : {HandMode::Holding, HandMode::Hidden, HandMode::Normal, HandMode::Knocking, HandMode::Totem,
	                        HandMode::Seed, HandMode::Pulling, HandMode::Temple})
	{
		EXPECT_EQ(tracker.Step(InMode(mode)).count, 1);
		EXPECT_EQ(tracker.Step(InMode(mode)).count, 0);
	}
}

TEST(HandBlend, InTheNormalModeEachChangeOfAnimationStartsABlend)
{
	ChangeTracker tracker;
	tracker.Step(InMode(HandMode::Camera));
	// The first animation after the hand enters the mode does not start a second blend
	EXPECT_EQ(tracker.Step(InMode(HandMode::Normal, Cycle::Stroke)).count, 1);
	EXPECT_EQ(tracker.Step(InMode(HandMode::Normal, Cycle::Wiggle)).count, 1);
	EXPECT_EQ(tracker.Step(InMode(HandMode::Normal, Cycle::HoldFingers)).count, 1);
	EXPECT_EQ(tracker.Step(InMode(HandMode::Normal, Cycle::HoldFingers)).count, 0);
	// Animations in other modes do not start a blend
	tracker.Step(InMode(HandMode::Holding, Cycle::HoldAbove));
	EXPECT_EQ(tracker.Step(InMode(HandMode::Holding, Cycle::HoldSide)).count, 0);
}

TEST(HandBlend, TheCameraModeStartsFromTheWiggleAndAnEmptyGripStartsTwoBlends)
{
	ChangeTracker tracker;
	tracker.Step(InMode(HandMode::Normal));
	// The mode changes and then the animation changes: the pose of the frame goes in place of the kept pose
	auto changes = tracker.Step(InMode(HandMode::Camera, Cycle::Rotate));
	EXPECT_EQ(changes.count, 2);
	EXPECT_TRUE(changes.poseIntoKept);
	changes = tracker.Step(InMode(HandMode::Camera, Cycle::Rotate));
	EXPECT_EQ(changes.count, 0);
	EXPECT_FALSE(changes.poseIntoKept);
	// A grip of the land with nothing below the hand starts two blends
	changes = tracker.Step(InMode(HandMode::Camera, Cycle::Grip));
	EXPECT_EQ(changes.count, 2);
	EXPECT_FALSE(changes.poseIntoKept);
	// A grip over a thing starts one
	tracker.Step(InMode(HandMode::Camera, Cycle::Wiggle));
	changes = tracker.Step({.mode = HandMode::Camera, .cycle = Cycle::Grip, .nothingUnderHand = false});
	EXPECT_EQ(changes.count, 1);
	EXPECT_TRUE(changes.poseIntoKept);
	// The wiggle as the hand enters the mode starts no second blend
	tracker.Step(InMode(HandMode::Normal));
	changes = tracker.Step(InMode(HandMode::Camera, Cycle::Wiggle));
	EXPECT_EQ(changes.count, 1);
	EXPECT_FALSE(changes.poseIntoKept);
}

TEST(HandBlend, OnACreatureOnlyTheTickleAndTheSlapStartABlend)
{
	ChangeTracker tracker;
	tracker.Step(InMode(HandMode::Normal));
	// The hand enters the mode with the tickle
	EXPECT_EQ(tracker.Step(InMode(HandMode::Creature, Cycle::Tickle)).count, 1);
	EXPECT_EQ(tracker.Step(InMode(HandMode::Creature, Cycle::Wiggle)).count, 0);
	EXPECT_EQ(tracker.Step(InMode(HandMode::Creature, Cycle::Tickle)).count, 1);
	EXPECT_EQ(tracker.Step(InMode(HandMode::Creature, Cycle::Slap)).count, 1);
	// After the slap, the wiggle and a second slap start no blend
	EXPECT_EQ(tracker.Step(InMode(HandMode::Creature, Cycle::Wiggle)).count, 0);
	EXPECT_EQ(tracker.Step(InMode(HandMode::Creature, Cycle::Slap)).count, 0);
	EXPECT_EQ(tracker.Step(InMode(HandMode::Creature, Cycle::Tickle)).count, 1);
}

TEST(HandBlend, AnotherThingInTheHandStartsABlend)
{
	ChangeTracker tracker;
	const auto first = static_cast<entt::entity>(1);
	const auto second = static_cast<entt::entity>(2);
	tracker.Step({.mode = HandMode::Holding, .held = first});
	EXPECT_EQ(tracker.Step({.mode = HandMode::Holding, .held = second}).count, 1);
	// A thing that the hand takes or drops changes the mode, which starts one blend
	EXPECT_EQ(tracker.Step({.mode = HandMode::Normal}).count, 1);
	EXPECT_EQ(tracker.Step({.mode = HandMode::Holding, .held = first}).count, 1);
}

TEST(HandBlend, TheCameraIconsShowForTheEmptyTheCameraAndTheHoldingHand)
{
	using openblack::hand_blend::AllowsCameraIcons;
	using openblack::hand_blend::HandMode;
	EXPECT_TRUE(AllowsCameraIcons(HandMode::Normal, false));
	EXPECT_TRUE(AllowsCameraIcons(HandMode::Camera, false));
	EXPECT_TRUE(AllowsCameraIcons(HandMode::Holding, false));
	EXPECT_TRUE(AllowsCameraIcons(HandMode::Seed, false));
	// Not while the hand scoops from a thing that it is locked onto
	EXPECT_FALSE(AllowsCameraIcons(HandMode::Holding, true));
	EXPECT_FALSE(AllowsCameraIcons(HandMode::Seed, true));
	EXPECT_TRUE(AllowsCameraIcons(HandMode::Normal, true));
	for (const auto mode :
	     {HandMode::Hidden, HandMode::Pulling, HandMode::Totem, HandMode::Creature, HandMode::Knocking, HandMode::Temple})
	{
		EXPECT_FALSE(AllowsCameraIcons(mode, false));
	}
}

TEST(HandBlend, TheChangeTellsTheModeThatTheHandLeft)
{
	ChangeTracker tracker;
	EXPECT_FALSE(tracker.Step({.mode = HandMode::Holding, .held = static_cast<entt::entity>(1)}).left.has_value());
	EXPECT_FALSE(tracker.Step({.mode = HandMode::Holding, .held = static_cast<entt::entity>(1)}).left.has_value());
	EXPECT_EQ(tracker.Step({.mode = HandMode::Normal}).left, HandMode::Holding);
	EXPECT_FALSE(tracker.Step({.mode = HandMode::Normal}).left.has_value());
	EXPECT_EQ(tracker.Step({.mode = HandMode::Creature}).left, HandMode::Normal);
}

TEST(HandBlend, TheNormalModeResetsAfterPlacingOnlyFromTheModesThatTheFrameKnowsLate)
{
	for (const auto left :
	     {HandMode::Hidden, HandMode::Pulling, HandMode::Holding, HandMode::Totem, HandMode::Creature, HandMode::Seed})
	{
		EXPECT_TRUE(ResetsAfterPlacing(left, HandMode::Normal));
	}
	// The camera mode, a knock and the temple reset before the hand places itself
	for (const auto left : {HandMode::Camera, HandMode::Knocking, HandMode::Temple, HandMode::Normal})
	{
		EXPECT_FALSE(ResetsAfterPlacing(left, HandMode::Normal));
	}
	// A different mode does not reset the eases of the normal mode
	EXPECT_FALSE(ResetsAfterPlacing(HandMode::Normal, HandMode::Holding));
	EXPECT_FALSE(ResetsAfterPlacing(HandMode::Holding, HandMode::Seed));
	EXPECT_FALSE(ResetsAfterPlacing(HandMode::Hidden, HandMode::Holding));
}
