/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "Debug/TestbedScenarioDeathWave.h"

using openblack::death_wave::SpreadPicks;

TEST(DeathWave, ThePicksSpreadEvenlyOverTheList)
{
	EXPECT_EQ(SpreadPicks(100, 4), (std::vector<size_t> {0, 25, 50, 75}));
	EXPECT_EQ(SpreadPicks(10, 3), (std::vector<size_t> {0, 3, 6}));
}

TEST(DeathWave, AShortListGivesAllItsPlaces)
{
	EXPECT_EQ(SpreadPicks(3, 5), (std::vector<size_t> {0, 1, 2}));
	EXPECT_EQ(SpreadPicks(4, 4), (std::vector<size_t> {0, 1, 2, 3}));
}

TEST(DeathWave, NoListOrNoCountGivesNoPicks)
{
	EXPECT_TRUE(SpreadPicks(0, 5).empty());
	EXPECT_TRUE(SpreadPicks(50, 0).empty());
}

TEST(DeathWave, EachPickIsADifferentPlaceInTheList)
{
	for (size_t size = 1; size < 200; size += 7)
	{
		for (size_t count = 1; count < 60; count += 5)
		{
			const auto picks = SpreadPicks(size, count);
			ASSERT_EQ(picks.size(), std::min(size, count));
			EXPECT_TRUE(std::ranges::is_sorted(picks));
			EXPECT_EQ(std::ranges::adjacent_find(picks), picks.end());
			EXPECT_LT(picks.back(), size);
		}
	}
}
