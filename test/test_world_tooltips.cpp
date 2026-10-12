/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Gui/WorldToolTipRules.h"

using namespace openblack::gui::world_tooltip;

TEST(WorldToolTips, BeliefLeftOrNeeded)
{
	auto left = Belief(0.5f);
	EXPECT_EQ(left.index, k_BeliefLeft);
	EXPECT_DOUBLE_EQ(left.first, 500.0);
	auto needed = Belief(-0.25f);
	EXPECT_EQ(needed.index, k_BeliefNeeded);
	EXPECT_DOUBLE_EQ(needed.first, 250.0);
	EXPECT_EQ(Belief(0.0f).index, k_BeliefLeft);
}

TEST(WorldToolTips, TempleHealthIsPercentage)
{
	EXPECT_DOUBLE_EQ(TempleHealth(1.0f).first, 100.0);
	EXPECT_EQ(Compose(k_TempleHealth, u"Temple Health: %3.0f%%", TempleHealth(0.755f).first, std::nullopt),
	          u"Temple Health:  76%");
}

TEST(WorldToolTips, PopulationAsPeopleOverPlaces)
{
	const auto population = Population(12, 34);
	EXPECT_EQ(Compose(population.index, u"Population: %3.0f", population.first, population.second), u"Population:  12/34");
}

TEST(WorldToolTips, StoredWritesFoodThenWood)
{
	const auto stored = Stored(5, 120);
	EXPECT_EQ(Compose(stored.index, u"Stored - Food: %3.0f Wood: %3.0f", stored.first, stored.second),
	          u"Stored - Food:   5 Wood: 120");
}

TEST(WorldToolTips, PilesAndHeldAmounts)
{
	EXPECT_EQ(PileAmount(true, 1).index, k_FoodAmount);
	EXPECT_EQ(PileAmount(false, 1).index, k_WoodAmount);
	EXPECT_EQ(HeldAmount(7).index, k_HeldAmount);
	EXPECT_EQ(Compose(k_HeldAmount, u"Amount: %3.0f", 7.0, std::nullopt), u"Amount:   7");
}
