/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The reactions to strokes and slaps that the hand sent, and how a reaction that waits for its turn stops a new one

#include <gtest/gtest.h>

#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureSentReactions.h"

using namespace openblack;
using creature_hand::SentReactions;
using creature_hand::TakesReaction;

namespace
{
constexpr auto k_Tiger = static_cast<entt::entity>(3);
constexpr auto k_Cow = static_cast<entt::entity>(7);
} // namespace

TEST(CreatureSentReactions, ACountedReactionWaitsUntilATurnDoesIt)
{
	SentReactions sent;
	EXPECT_FALSE(sent.Waiting(k_Tiger));
	sent.Sent(k_Tiger, true);
	EXPECT_TRUE(sent.Waiting(k_Tiger));
	EXPECT_FALSE(sent.Waiting(k_Cow));
	sent.Done();
	EXPECT_FALSE(sent.Waiting(k_Tiger));
	EXPECT_TRUE(sent.Empty());
}

TEST(CreatureSentReactions, AReactionThatTheHandDidNotCountDoesNotWait)
{
	// A stroke that the creature refused still pulls the face, so the hand sends it, but it does not count
	SentReactions sent;
	sent.Sent(k_Tiger, false);
	EXPECT_FALSE(sent.Waiting(k_Tiger));
	EXPECT_FALSE(sent.Empty());
}

TEST(CreatureSentReactions, TheTurnsDoTheReactionsInTheOrderThatTheHandSentThem)
{
	SentReactions sent;
	sent.Sent(k_Tiger, false);
	sent.Sent(k_Tiger, true);
	// The turn did the refused stroke: the counted one still waits
	sent.Done();
	EXPECT_TRUE(sent.Waiting(k_Tiger));
	sent.Done();
	EXPECT_FALSE(sent.Waiting(k_Tiger));
	// A turn that does a reaction of this computer with none sent changes nothing
	sent.Done();
	EXPECT_TRUE(sent.Empty());
}

TEST(CreatureSentReactions, ClearForgetsAllTheReactions)
{
	SentReactions sent;
	sent.Sent(k_Tiger, true);
	sent.Sent(k_Cow, true);
	sent.Clear();
	EXPECT_FALSE(sent.Waiting(k_Tiger));
	EXPECT_FALSE(sent.Waiting(k_Cow));
}

TEST(CreatureSentReactions, AWaitingReactionStopsASlapAndAStroke)
{
	// The slap comes a frame after a stroke that waits for its turn: the creature does not reel and the slap does not count
	EXPECT_FALSE(TakesReaction(true, creature_feedback::k_SlapInterruptsAfter, true));
	EXPECT_FALSE(TakesReaction(true, creature_feedback::k_StrokeInterruptsAfter, true));
	// With no reaction that waits, the body decides
	EXPECT_TRUE(TakesReaction(false, creature_feedback::k_SlapInterruptsAfter, true));
	EXPECT_FALSE(TakesReaction(false, creature_feedback::k_SlapInterruptsAfter, false));
}

TEST(CreatureSentReactions, AReactionWithNoLimitInterruptsAWaitingReaction)
{
	// A reaction that interrupts an action at any point also interrupts one at its start
	EXPECT_TRUE(TakesReaction(true, 0.0f, true));
	// The body still has to take it
	EXPECT_FALSE(TakesReaction(true, 0.0f, false));
}
