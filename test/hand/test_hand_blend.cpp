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
