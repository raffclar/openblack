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

#include "Creature/CreatureFizz.h"

using namespace openblack::creature_fizz;

namespace
{
constexpr uint32_t k_TurnMs = 100;
} // namespace

TEST(CreatureFizz, GoingRightOutOfSightSounds)
{
	EXPECT_TRUE(SetFizz({}, 1.0f, 2.0f, false).sound);
	EXPECT_TRUE(SetFizz({}, 1.0f, 0.0f, false).sound);
	// Only exactly right out: not back in, part way, or beyond before it is kept within bounds
	EXPECT_FALSE(SetFizz({.now = 1.0f}, 0.0f, 2.0f, false).sound);
	EXPECT_FALSE(SetFizz({}, 0.75f, 2.0f, false).sound);
	EXPECT_FALSE(SetFizz({}, 1.5f, 2.0f, false).sound);
}

TEST(CreatureFizz, OverSomeSecondsItMovesAShareASecond)
{
	const auto out = SetFizz({}, 1.0f, 2.0f, false).fizz;
	EXPECT_EQ(out, (Fizz {.now = 0.0f, .target = 1.0f, .perSecond = 0.5f}));
	const auto in = SetFizz({.now = 1.0f}, 0.0f, 3.0f, false).fizz;
	EXPECT_FLOAT_EQ(in.perSecond, -1.0f / 3.0f);
	EXPECT_EQ(in.now, 1.0f);
}

TEST(CreatureFizz, OverNoTimeOrWhenThereItIsThereAtOnce)
{
	EXPECT_EQ(SetFizz({.now = 0.3f, .target = 1.0f, .perSecond = 0.5f}, 0.0f, 0.0f, false).fizz,
	          (Fizz {.now = 0.0f, .target = 0.0f, .perSecond = 0.0f}));
	EXPECT_EQ(SetFizz({.now = 1.0f}, 1.0f, 2.0f, false).fizz, (Fizz {.now = 1.0f, .target = 1.0f, .perSecond = 0.0f}));
}

TEST(CreatureFizz, TargetIsKeptWithinBounds)
{
	EXPECT_EQ(SetFizz({}, 2.0f, 0.0f, false).fizz.now, 1.0f);
	EXPECT_EQ(SetFizz({.now = 1.0f}, -1.0f, 0.0f, false).fizz.now, 0.0f);
}

TEST(CreatureFizz, StepsReachTheTargetAndStopPastIt)
{
	auto fizz = SetFizz({}, 1.0f, 2.0f, false).fizz;
	int turns = 0;
	while (fizz.perSecond != 0.0f && turns < 100)
	{
		fizz = Step(fizz, k_TurnMs);
		++turns;
	}
	// A twentieth a turn, a hair over in single precision: past the target on the twentieth turn, and stopped there
	EXPECT_EQ(fizz.now, 1.0f);
	EXPECT_EQ(turns, 20);

	fizz = SetFizz(fizz, 0.0f, 2.0f, false).fizz;
	for (int i = 0; i < 25; ++i)
	{
		fizz = Step(fizz, k_TurnMs);
	}
	EXPECT_EQ(fizz.now, 0.0f);
	EXPECT_EQ(fizz.perSecond, 0.0f);
	EXPECT_TRUE(Settled(fizz));
}

TEST(CreatureFizz, StillFizzDoesNotMove)
{
	const Fizz fizz {.now = 0.4f, .target = 0.4f};
	EXPECT_EQ(Step(fizz, k_TurnMs), fizz);
	EXPECT_FALSE(Settled(fizz));
}

TEST(CreatureFizz, GoingForGoodIsFinal)
{
	auto fizz = SetFizz({}, 1.0f, 2.0f, true).fizz;
	EXPECT_TRUE(fizz.goesForGood);
	EXPECT_FALSE(Gone(fizz));
	// Nothing brings it back, and nothing sounds again
	const auto again = SetFizz(fizz, 0.0f, 0.0f, false);
	EXPECT_EQ(again.fizz, fizz);
	EXPECT_FALSE(SetFizz(fizz, 1.0f, 0.0f, false).sound);
	for (int i = 0; i < 25; ++i)
	{
		fizz = Step(fizz, k_TurnMs);
	}
	EXPECT_TRUE(Gone(fizz));
	EXPECT_FALSE(Settled(fizz));
}

TEST(CreatureFizz, TurnsOfSecondsUseWholeTurnsASecond)
{
	EXPECT_EQ(TurnsOf(2.0f, 100), 20);
	EXPECT_EQ(TurnsOf(3.0f, 100), 30);
	// Turns a second are a whole number first: 1000 / 300 is 3
	EXPECT_EQ(TurnsOf(2.0f, 300), 6);
	EXPECT_EQ(TurnsOf(2.5f, 100), 25);
	EXPECT_EQ(TurnsOf(2.0f, 0), 0);
}
