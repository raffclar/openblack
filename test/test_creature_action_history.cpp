/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureActionHistory.h"

using namespace openblack;
using namespace openblack::creature_action_history;

TEST(CreatureActionHistory, AFreshCreatureDidEachActionUpTo119TurnsAgo)
{
	uint32_t next = 0;
	const auto turns = Fresh(4, [&next](uint32_t range) {
		EXPECT_EQ(range, k_FreshTurnsRange);
		return next++ * 50 % range;
	});
	EXPECT_EQ(turns, (std::vector<uint32_t> {0, 50, 100, 30}));
}

TEST(CreatureActionHistory, TheEndedActionIsNewAndTheOthersOfItsDesireGetOlder)
{
	std::vector<uint32_t> turns {7, 7, 7, 7, 7, 7};
	const std::array<uint32_t, 4> list {2, 3, 5, 4};
	PlanEnded(turns, 3, list, 20);
	EXPECT_EQ(turns, (std::vector<uint32_t> {7, 7, 27, 0, 27, 27}));
}

TEST(CreatureActionHistory, TheListEndsAtItsFirstEntryThatIsNoAction)
{
	std::vector<uint32_t> turns {0, 1, 1, 1};
	const std::array<uint32_t, 4> list {1, 0, 2, 3};
	PlanEnded(turns, 3, list, 5);
	EXPECT_EQ(turns, (std::vector<uint32_t> {0, 6, 1, 0}));
}

TEST(CreatureActionHistory, OnlyTheFirstThirtyActionsOfTheListGetOlder)
{
	std::vector<uint32_t> turns(40, 0);
	std::vector<uint32_t> list;
	for (uint32_t a = 1; a < 40; ++a)
	{
		list.push_back(a);
	}
	PlanEnded(turns, 0, list, 1);
	EXPECT_EQ(turns.at(30), 1u);
	EXPECT_EQ(turns.at(31), 0u);
}
