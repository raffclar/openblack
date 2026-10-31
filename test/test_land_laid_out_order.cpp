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
#include <set>

#include <gtest/gtest.h>

#include "LandLaidOutOrder.h"

using namespace openblack::land_laid_out;

TEST(LandLaidOutOrder, EachStepComesOneTime)
{
	const std::set<Step> steps(k_Order.begin(), k_Order.end());
	EXPECT_EQ(steps.size(), k_Order.size());
	EXPECT_EQ(PlaceOf(Step::FillSlots), k_Order.size() - 1);
}

TEST(LandLaidOutOrder, TheTownsGetTheirForestsBeforeTheWorshipSites)
{
	EXPECT_LT(PlaceOf(Step::ScenicForests), PlaceOf(Step::ForestsToTowns));
	EXPECT_LT(PlaceOf(Step::ForestsToTowns), PlaceOf(Step::WorshipSites));
	EXPECT_LT(PlaceOf(Step::WorshipSites), PlaceOf(Step::InfluencePower));
}

TEST(LandLaidOutOrder, TheFootpathsLoadAfterTheInfluencePower)
{
	EXPECT_LT(PlaceOf(Step::PlaceThings), PlaceOf(Step::FootpathObstacles));
	EXPECT_LT(PlaceOf(Step::FootpathObstacles), PlaceOf(Step::ScenicForests));
	EXPECT_LT(PlaceOf(Step::InfluencePower), PlaceOf(Step::FootpathFile));
}

TEST(LandLaidOutOrder, TheSlotsFillAfterTheLandIsLaidOut)
{
	const auto fill = PlaceOf(Step::FillSlots);
	EXPECT_TRUE(std::ranges::all_of(k_Order, [fill](Step step) { return step == Step::FillSlots || PlaceOf(step) < fill; }));
}
