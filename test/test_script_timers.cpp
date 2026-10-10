/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ScriptHeaders/ScriptTimers.h"

using namespace openblack::script::timers;

TEST(ScriptTimers, ATimerRunsForWholeTurns)
{
	// Ten turns a second, cut down
	EXPECT_EQ(TurnsFor(3.0f), 30);
	EXPECT_EQ(TurnsFor(1.25f), 12);
	EXPECT_EQ(TurnsFor(0.05f), 0);
	EXPECT_EQ(TurnsFor(-1.0f), -10);
}

TEST(ScriptTimers, TimeRemainingRunsDownToNothing)
{
	const auto timer = Set(100, 2.0f);
	EXPECT_FLOAT_EQ(SecondsRemaining(timer, 100), 2.0f);
	EXPECT_FLOAT_EQ(SecondsRemaining(timer, 105), 1.5f);
	EXPECT_FLOAT_EQ(SecondsRemaining(timer, 120), 0.0f);
	// Never below nothing
	EXPECT_FLOAT_EQ(SecondsRemaining(timer, 200), 0.0f);
}

TEST(ScriptTimers, TimeSinceSetCountsOnPastTheEnd)
{
	const auto timer = Set(100, 2.0f);
	EXPECT_FLOAT_EQ(SecondsSinceSet(timer, 100), 0.0f);
	EXPECT_FLOAT_EQ(SecondsSinceSet(timer, 135), 3.5f);
}
