/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/MagicTreeWatchRules.h"

using namespace openblack::ecs::magic_tree_watch;

TEST(MagicTreeWatch, TheFirstTreeIsRolledFromAllButOneAndTheTreesGoRound)
{
	EXPECT_EQ(FirstTreeRollRange(18), 17u);
	EXPECT_EQ(FirstTreeRollRange(1), 0u);
	EXPECT_EQ(NextTree(4, 18), 5u);
	EXPECT_EQ(NextTree(17, 18), 0u);
}

TEST(MagicTreeWatch, ItLooksAtTreesUntilAllLessAFreshRollOfUpToHalf)
{
	EXPECT_EQ(SkippedRollRange(18), 9u);
	EXPECT_EQ(SkippedRollRange(1), 0u);
	EXPECT_TRUE(LooksAtAnother(0, 18, 8));
	EXPECT_TRUE(LooksAtAnother(9, 18, 8));
	EXPECT_FALSE(LooksAtAnother(10, 18, 8));
	EXPECT_TRUE(LooksAtAnother(17, 18, 0));
	EXPECT_FALSE(LooksAtAnother(18, 18, 0));
	EXPECT_TRUE(LooksAtAnother(0, 1, 0));
	EXPECT_FALSE(LooksAtAnother(1, 1, 0));
}

TEST(MagicTreeWatch, TheGrowingTreesComeBeforeTheGrownOnes)
{
	const auto first = SlotOf(2, 5);
	EXPECT_TRUE(first.growing);
	EXPECT_EQ(first.index, 2u);
	const auto later = SlotOf(7, 5);
	EXPECT_FALSE(later.growing);
	EXPECT_EQ(later.index, 2u);
	const auto none = SlotOf(0, 0);
	EXPECT_FALSE(none.growing);
	EXPECT_EQ(none.index, 0u);
}

TEST(MagicTreeWatch, ALookLastsTwoToJustUnderFiveSeconds)
{
	EXPECT_EQ(LookRollRange(10), 30u);
	EXPECT_EQ(LookTurns(0, 10), 20);
	EXPECT_EQ(LookTurns(29, 10), 49);
	EXPECT_EQ(LookCounter(20), 0x1414);
}

TEST(MagicTreeWatch, TheLookChangesHalfWayAndCountsDownInEightBits)
{
	auto counter = LookCounter(21);
	EXPECT_FALSE(ChangesLookNow(counter));
	uint32_t turns = 0;
	while (!ChangesLookNow(counter))
	{
		counter = CountDown(counter);
		++turns;
	}
	// 21 halved is 10: the change comes when 10 turns are left
	EXPECT_EQ(turns, 11u);
	EXPECT_EQ(counter & 0xFFu, 10u);
	EXPECT_EQ(counter >> 8u, 21u);
	EXPECT_EQ(CountDown(LookCounter(0)), 0x00FF);
}
