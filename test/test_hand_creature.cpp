/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// hand_creature: the hand's locked select on a creature, its time with it, the click and the feedback, frame by frame
// with fake facts. Without a creature the hand's state follows the required state as before

#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "ECS/HandCreature.h"

using namespace openblack::ecs::hand_creature;

namespace
{
constexpr int32_t k_Normal = 1;

LockFacts Facts(bool locked, bool actionReleased = false)
{
	return {.available = true, .locked = locked, .stillSelected = true, .actionReleased = actionReleased};
}

std::vector<Send> SendsOf(const FrameStep& step)
{
	return {step.Sends().begin(), step.Sends().end()};
}

/// A frame of a creature held in the hand, the interface in Creature Interaction
FrameFacts Holding(uint32_t cameraMs, int32_t renderState = k_CreatureHandState)
{
	return {
	    .creatureInteraction = true,
	    .hasCreature = true,
	    .cameraMs = cameraMs,
	    .requiredState = k_CreatureHandState,
	    .renderState = renderState,
	    .creatureLocked = true,
	};
}

/// The first frame after the interface left Creature Interaction
FrameFacts LettingGo(uint32_t cameraMs)
{
	return {
	    .creatureInteraction = false,
	    .hasCreature = true,
	    .cameraMs = cameraMs,
	    .requiredState = k_Normal,
	    .renderState = k_CreatureHandState,
	    .creatureLocked = true,
	};
}

const auto k_Own = []() { return true; };
const auto k_NotOwn = []() { return false; };
} // namespace

TEST(HandCreatureLock, WaitsForTheTurnToLockThenHolds)
{
	const auto waiting = StepLock({}, Facts(false));
	EXPECT_EQ(waiting.next.phase, LockPhase::Waiting);
	EXPECT_FALSE(waiting.sendEnd);
	EXPECT_FALSE(waiting.letGo);
	const auto held = StepLock(waiting.next, Facts(true));
	EXPECT_EQ(held.next.phase, LockPhase::Held);
	EXPECT_FALSE(held.sendEnd);
	EXPECT_FALSE(held.letGo);
}

TEST(HandCreatureLock, ALockTheTurnRefusedIsLetGo)
{
	const auto step = StepLock({}, {.available = true, .locked = false, .stillSelected = false});
	EXPECT_TRUE(step.letGo);
	EXPECT_FALSE(step.sendEnd);
}

TEST(HandCreatureLock, TheReleaseEndsItOneFrameLaterAndTheAppliedEndLetsGo)
{
	LockState state {.phase = LockPhase::Held};
	auto step = StepLock(state, Facts(true, true));
	EXPECT_EQ(step.next.phase, LockPhase::WaitingForLockOff);
	EXPECT_FALSE(step.sendEnd);
	step = StepLock(step.next, Facts(true));
	EXPECT_EQ(step.next.phase, LockPhase::Completion);
	EXPECT_TRUE(step.sendEnd);
	EXPECT_FALSE(step.letGo);
	step = StepLock(step.next, Facts(true));
	EXPECT_EQ(step.next.phase, LockPhase::Completion);
	EXPECT_FALSE(step.sendEnd);
	EXPECT_FALSE(step.letGo);
	step = StepLock(step.next, Facts(false));
	EXPECT_TRUE(step.letGo);
	EXPECT_FALSE(step.sendEnd);
}

TEST(HandCreatureLock, AReleaseBeforeTheLockIsKeptUntilHeld)
{
	// the button goes up while the turn has not locked the creature yet: the held phase still reads it
	auto step = StepLock({}, Facts(false, true));
	EXPECT_EQ(step.next.phase, LockPhase::Waiting);
	EXPECT_TRUE(step.next.released);
	step = StepLock(step.next, Facts(true));
	EXPECT_EQ(step.next.phase, LockPhase::Held);
	step = StepLock(step.next, Facts(true));
	EXPECT_EQ(step.next.phase, LockPhase::WaitingForLockOff);
}

TEST(HandCreatureLock, AHeldCreatureNoLongerLockedIsLetGo)
{
	const auto step = StepLock({.phase = LockPhase::Held}, Facts(false));
	EXPECT_TRUE(step.letGo);
	EXPECT_FALSE(step.sendEnd);
}

TEST(HandCreatureLock, ACreatureGoneEndsItUnlessTheEndIsSent)
{
	for (const auto phase : {LockPhase::Waiting, LockPhase::Held, LockPhase::WaitingForLockOff})
	{
		const auto step = StepLock({.phase = phase}, {.available = false, .locked = true, .stillSelected = true});
		EXPECT_TRUE(step.sendEnd) << static_cast<int>(phase);
		EXPECT_TRUE(step.letGo) << static_cast<int>(phase);
	}
	const auto step = StepLock({.phase = LockPhase::Completion}, {.available = false, .locked = true});
	EXPECT_FALSE(step.sendEnd);
	EXPECT_TRUE(step.letGo);
}

TEST(HandCreatureFrame, TheCreatureStateWaitsForTheLock)
{
	auto facts = Holding(16, k_Normal);
	facts.creatureLocked = false;
	EXPECT_EQ(StepFrame({}, facts, k_Own).renderState, k_Normal);
	facts.creatureLocked = true;
	const auto step = StepFrame({}, facts, k_Own);
	EXPECT_EQ(step.renderState, k_CreatureHandState);
	EXPECT_TRUE(step.Sends().empty());
}

TEST(HandCreatureFrame, ALockLostInTheCreatureStateDoesNotHoldIt)
{
	// the lock is needed to enter the state, not to stay in it
	auto facts = Holding(16);
	facts.creatureLocked = false;
	EXPECT_EQ(StepFrame({.started = true}, facts, k_Own).renderState, k_CreatureHandState);
}

TEST(HandCreatureFrame, TheTimeStartsAtTheInteractionsFirstFrame)
{
	auto step = StepFrame({.cameraMs = 5000}, Holding(16), k_Own);
	EXPECT_TRUE(step.interaction.started);
	EXPECT_EQ(step.interaction.cameraMs, 0u);
	step = StepFrame(step.interaction, Holding(16), k_Own);
	EXPECT_EQ(step.interaction.cameraMs, 16u);
}

TEST(HandCreatureFrame, ALetGoWithinAClickOfTheOwnCreatureSendsTheClickThenTheFeedback)
{
	auto step = StepFrame({}, Holding(16), k_Own);
	step = StepFrame(step.interaction, Holding(200), k_Own);
	step = StepFrame(step.interaction, LettingGo(200), k_Own);
	EXPECT_FALSE(step.interaction.started);
	EXPECT_EQ(step.interaction.cameraMs, 400u);
	EXPECT_EQ(step.renderState, k_Normal);
	EXPECT_EQ(SendsOf(step), (std::vector<Send> {Send::Click, Send::Feedback}));
}

TEST(HandCreatureFrame, ALongHoldSendsOnlyTheFeedback)
{
	auto step = StepFrame({}, Holding(16), k_Own);
	step = StepFrame(step.interaction, Holding(300), k_Own);
	step = StepFrame(step.interaction, LettingGo(150), k_Own);
	EXPECT_EQ(step.interaction.cameraMs, 450u);
	EXPECT_EQ(SendsOf(step), (std::vector<Send> {Send::Feedback}));
}

TEST(HandCreatureFrame, AnotherPlayersCreatureIsNoClick)
{
	auto step = StepFrame({}, Holding(16), k_NotOwn);
	step = StepFrame(step.interaction, LettingGo(16), k_NotOwn);
	EXPECT_EQ(SendsOf(step), (std::vector<Send> {Send::Feedback}));
}

TEST(HandCreatureFrame, TheOwnerIsAskedOnlyWhenTheInteractionEnds)
{
	int asked = 0;
	const auto own = [&asked]() {
		++asked;
		return true;
	};
	auto step = StepFrame({}, Holding(16), own);
	step = StepFrame(step.interaction, Holding(16), own);
	EXPECT_EQ(asked, 0);
	step = StepFrame(step.interaction, LettingGo(16), own);
	EXPECT_EQ(asked, 1);
	step = StepFrame(step.interaction, {.requiredState = k_Normal, .renderState = k_Normal}, own);
	EXPECT_EQ(asked, 1);
}

TEST(HandCreatureFrame, WithoutACreatureTheStateFollowsTheRequiredOne)
{
	// no creature: no time started, nothing sent, and the hand takes every required state but CREATURE at once
	for (int32_t required = 0; required < 12; ++required)
	{
		for (const int32_t current : {0, 1, 5, 9})
		{
			const auto step = StepFrame({}, {.cameraMs = 16, .requiredState = required, .renderState = current}, k_NotOwn);
			EXPECT_EQ(step.renderState, required == k_CreatureHandState ? current : required) << required << " " << current;
			EXPECT_TRUE(step.Sends().empty());
			EXPECT_FALSE(step.interaction.started);
		}
	}
}
