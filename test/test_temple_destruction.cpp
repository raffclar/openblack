/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "Temple/TempleDestruction.h"

using namespace openblack::temple_destruction;

namespace
{
/// Every turn's events at a tenth of a second a turn, from the first turn on, until the end
std::vector<Events> RunToEnd()
{
	std::vector<Events> turns;
	float clock = 0.0f;
	for (int turn = 0; turn < 400; ++turn)
	{
		const float before = clock;
		clock += 100.0f * 0.001f;
		turns.push_back(Between(before, clock));
		if (turns.back().end)
		{
			break;
		}
	}
	return turns;
}
} // namespace

TEST(TempleDestruction, EachMomentHappensOnceOnTheTurnTheClockPassesIt)
{
	const auto turns = RunToEnd();
	const auto count = [&turns](auto field) {
		int n = 0;
		for (const auto& events : turns)
		{
			n += (events.*field) ? 1 : 0;
		}
		return n;
	};
	EXPECT_EQ(count(&Events::loopStarts), 1);
	EXPECT_EQ(count(&Events::glow), 1);
	EXPECT_EQ(count(&Events::explosion), 1);
	EXPECT_EQ(count(&Events::smoke), 1);
	EXPECT_EQ(count(&Events::end), 1);
	// The loop on the first turn, the end with the smoke on the same last turn, about 22 seconds on
	const bool loopsFirst = turns.front().loopStarts;
	const bool smokesLast = turns.back().smoke;
	EXPECT_TRUE(loopsFirst);
	EXPECT_TRUE(smokesLast);
	EXPECT_GE(turns.size(), 220u);
	EXPECT_LE(turns.size(), 221u);
}

TEST(TempleDestruction, AMomentIsPassedFromAtItToBeforeItAfter)
{
	EXPECT_TRUE(Passes(0.0f, 0.0f, 0.1f));
	EXPECT_FALSE(Passes(0.0f, 0.1f, 0.1f));
	EXPECT_FALSE(Passes(0.1f, 0.0f, 0.2f));
}

TEST(TempleDestruction, TheHeartFadesFromFourteenSecondsOverFour)
{
	EXPECT_EQ(HeartAlpha(0.0f), 255);
	EXPECT_EQ(HeartAlpha(14.0f), 255);
	EXPECT_EQ(HeartAlpha(16.0f), 127);
	EXPECT_EQ(HeartAlpha(18.0f), 0);
	EXPECT_EQ(HeartAlpha(21.0f), 0);
}

TEST(TempleDestruction, SpotVisualsLastTheirSecondsInTurns)
{
	EXPECT_EQ(TurnsFor(100, k_GlowSeconds), 60);
	EXPECT_EQ(TurnsFor(100, k_ExplosionSeconds), 150);
	EXPECT_EQ(SmokeTurns(100, 0.0f), 60);
	EXPECT_EQ(SmokeTurns(100, 0.4f), 90);
}

TEST(TempleDestruction, OnlyTheLocalPlayersLossOutsideSkirmishesAndOnlineGamesEndsTheGameOnce)
{
	EXPECT_TRUE(GameOverStarts({.localTempleDestroying = true}));
	EXPECT_FALSE(GameOverStarts({}));
	EXPECT_FALSE(GameOverStarts({.over = true, .localTempleDestroying = true}));
	EXPECT_FALSE(GameOverStarts({.skirmish = true, .localTempleDestroying = true}));
	EXPECT_FALSE(GameOverStarts({.multiplayer = true, .localTempleDestroying = true}));
}
