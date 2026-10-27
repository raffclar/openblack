/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include "Online/ResyncRule.h"
#include "Online/TurnPlayout.h"

using openblack::online::ResyncRule;
using openblack::online::TurnPlayout;

namespace
{
constexpr int64_t k_Turn = TurnPlayout::k_TurnMicroseconds;
}

TEST(TurnPlayout, ASteadyNetworkGivesNoBuffer)
{
	TurnPlayout playout;
	for (uint32_t turn = 0; turn < 50; ++turn)
	{
		playout.Arrived(turn, 1'000'000 + turn * k_Turn);
	}
	EXPECT_EQ(playout.JitterMicroseconds(), 0);
	EXPECT_EQ(playout.BufferMicroseconds(), 0);
	// Each turn is due as it arrives, and not before it arrives
	EXPECT_TRUE(playout.Due(49, 1'000'000 + 49 * k_Turn));
	EXPECT_FALSE(playout.Due(50, 1'000'000 + 60 * k_Turn));
}

TEST(TurnPlayout, JitterMakesTheBufferLargerAndKeepsTheTurnsSteady)
{
	TurnPlayout playout;
	// Every other turn comes 40 ms late
	for (uint32_t turn = 0; turn < 200; ++turn)
	{
		playout.Arrived(turn, turn * k_Turn + (turn % 2 == 1 ? 40'000 : 0));
	}
	EXPECT_GT(playout.JitterMicroseconds(), 30'000);
	EXPECT_EQ(playout.BufferMicroseconds(), playout.JitterMicroseconds() * TurnPlayout::k_JitterFactor);
	// The line follows the early turns, so a late turn is due at its steady time plus the buffer
	const auto due = playout.DueTime(199);
	ASSERT_TRUE(due.has_value());
	EXPECT_EQ(*due, 199 * k_Turn + playout.BufferMicroseconds());
	EXPECT_FALSE(playout.Due(199, *due - 1));
	EXPECT_TRUE(playout.Due(199, *due));
}

TEST(TurnPlayout, TheBufferHasALimit)
{
	TurnPlayout playout;
	for (uint32_t turn = 0; turn < 200; ++turn)
	{
		playout.Arrived(turn, turn * k_Turn + (turn % 2 == 1 ? 900'000 : 0));
	}
	EXPECT_EQ(playout.BufferMicroseconds(), TurnPlayout::k_MostBufferMicroseconds);
}

TEST(TurnPlayout, TurnsThatCameAtOnceAreAllDueForACatchUp)
{
	TurnPlayout playout;
	// After a loss the server sends the missed turns together
	for (uint32_t turn = 10; turn < 20; ++turn)
	{
		playout.Arrived(turn, 5'000'000);
	}
	for (uint32_t turn = 10; turn < 20; ++turn)
	{
		EXPECT_TRUE(playout.Due(turn, 5'000'000 + playout.BufferMicroseconds())) << turn;
	}
	playout.Done(15);
	EXPECT_FALSE(playout.Due(15, 9'000'000));
	EXPECT_TRUE(playout.Due(16, 9'000'000));
}

TEST(ResyncRule, ThreeQuickRejoinsThenTheClientGivesUp)
{
	ResyncRule rule;
	EXPECT_EQ(rule.Differ(10), ResyncRule::Action::Rejoin);
	EXPECT_EQ(rule.GetState(), ResyncRule::State::Resyncing);
	// Compares during the rejoin do not count
	EXPECT_EQ(rule.Differ(11), ResyncRule::Action::None);
	rule.Rejoined();
	EXPECT_EQ(rule.Differ(12), ResyncRule::Action::Rejoin);
	rule.Rejoined();
	EXPECT_EQ(rule.Differ(13), ResyncRule::Action::Rejoin);
	EXPECT_EQ(rule.Tries(), 3u);
	rule.Rejoined();
	EXPECT_EQ(rule.Differ(14), ResyncRule::Action::GiveUp);
	EXPECT_EQ(rule.GetState(), ResyncRule::State::OutOfSync);
	EXPECT_EQ(rule.FirstTurn(), 10u);
	EXPECT_EQ(rule.Differ(15), ResyncRule::Action::None);
}

TEST(ResyncRule, ARejoinThatWorksPutsTheClientBackInStep)
{
	ResyncRule rule;
	EXPECT_EQ(rule.Differ(10), ResyncRule::Action::Rejoin);
	rule.Same();
	EXPECT_EQ(rule.GetState(), ResyncRule::State::Resyncing);
	rule.Rejoined();
	rule.Same();
	EXPECT_EQ(rule.GetState(), ResyncRule::State::InStep);
	EXPECT_EQ(rule.Tries(), 0u);
	EXPECT_FALSE(rule.FirstTurn().has_value());
}
