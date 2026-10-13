/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <iterator>
#include <vector>

#include <gtest/gtest.h>

#include "Debug/TempleVillagersSurvey.h"

using namespace openblack::temple_villagers;

namespace
{
struct Listed
{
	Standing standing;
	float distance;
	entt::entity entity;
};
} // namespace

TEST(TempleVillagersSurvey, CountedAtTheSiteIsWorshippingWhereverItIs)
{
	EXPECT_EQ(StandingOf(true, false, 500.0f, 10.0f), Standing::Worshipping);
	// Counted at worship and still marked on its way, it is worshipping
	EXPECT_EQ(StandingOf(true, true, 0.0f, 10.0f), Standing::Worshipping);
}

TEST(TempleVillagersSurvey, OnItsWayIsListedFromAnywhere)
{
	EXPECT_EQ(StandingOf(false, true, 500.0f, 10.0f), Standing::OnTheWay);
}

TEST(TempleVillagersSurvey, OthersAreListedOnlyWithinTheRadius)
{
	EXPECT_EQ(StandingOf(false, false, 10.0f, 10.0f), Standing::Nearby);
	EXPECT_EQ(StandingOf(false, false, 9.0f, 10.0f), Standing::Nearby);
	EXPECT_FALSE(StandingOf(false, false, 10.5f, 10.0f).has_value());
	EXPECT_FALSE(StandingOf(false, false, 1.0f, 0.0f).has_value());
}

TEST(TempleVillagersSurvey, WorshippersComeFirstThenThoseOnTheWayThenTheNearestNearby)
{
	std::vector<Listed> list {
	    {Standing::Nearby, 5.0f, entt::entity {1}},       {Standing::OnTheWay, 80.0f, entt::entity {2}},
	    {Standing::Worshipping, 20.0f, entt::entity {3}}, {Standing::Nearby, 2.0f, entt::entity {4}},
	    {Standing::Worshipping, 10.0f, entt::entity {6}}, {Standing::Worshipping, 10.0f, entt::entity {5}},
	};
	std::ranges::sort(list, [](const Listed& a, const Listed& b) {
		return ListedBefore(a.standing, a.distance, a.entity, b.standing, b.distance, b.entity);
	});
	std::vector<entt::entity> order;
	std::ranges::transform(list, std::back_inserter(order), &Listed::entity);
	EXPECT_EQ(order, (std::vector<entt::entity> {entt::entity {5}, entt::entity {6}, entt::entity {3}, entt::entity {2},
	                                             entt::entity {4}, entt::entity {1}}));
}

TEST(TempleVillagersSurvey, NamesTheStandingsAndPlaces)
{
	EXPECT_EQ(Name(Standing::OnTheWay), "on the way");
	EXPECT_EQ(Name(SitePlace::Gate), "gate");
	EXPECT_EQ(Name(SitePlace::Centre), "centre");
}
