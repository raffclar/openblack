/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Common/GUtilsDistance.h"
#include "ECS/WorshipperRules.h"

namespace rules = openblack::ecs::worshipper_rules;

TEST(WorshipperRules, TargetRoundsAndWantsAtLeastOne)
{
	EXPECT_EQ(rules::Target(40, 0.0f), 0);
	EXPECT_EQ(rules::Target(40, -0.1f), 0);
	// Any share wants at least one, even of an empty town
	EXPECT_EQ(rules::Target(40, 0.001f), 1);
	EXPECT_EQ(rules::Target(0, 0.5f), 1);
	// Rounded to the nearest person
	EXPECT_EQ(rules::Target(40, 0.25f), 10);
	EXPECT_EQ(rules::Target(10, 0.25f), 3);
	EXPECT_EQ(rules::Target(10, 0.24f), 2);
	EXPECT_EQ(rules::Target(10, 1.0f), 10);
}

TEST(WorshipperRules, NeededCountsWhatIsAskedFor)
{
	const rules::Headcount town {.population = 20, .worshipping = 3, .onTheWay = 2, .askingToGoHome = 1, .share = 0.5f};
	EXPECT_EQ(rules::WorshippersNeeded(town, true, true).count, 10 - 5 + 1);
	EXPECT_EQ(rules::WorshippersNeeded(town, false, false).count, 10 - 3);
	EXPECT_EQ(rules::WorshippersNeeded(town, true, false).count, 5);
	EXPECT_FALSE(rules::WorshippersNeeded(town, true, true).alreadyEnough);
}

TEST(WorshipperRules, NeededIsBelowNoneWhenTooManyWorship)
{
	const rules::Headcount town {.population = 20, .worshipping = 8, .onTheWay = 0, .askingToGoHome = 0, .share = 0.25f};
	EXPECT_EQ(rules::WorshippersNeeded(town, true, true).count, -3);
	// No share sends everyone home
	const rules::Headcount none {.population = 20, .worshipping = 4, .share = 0.0f};
	EXPECT_EQ(rules::WorshippersNeeded(none, false, false).count, -4);
}

TEST(WorshipperRules, AlreadyEnoughOnlyWhenMakingUpForThoseGoingHome)
{
	const rules::Headcount town {.population = 20, .worshipping = 5, .onTheWay = 0, .askingToGoHome = 2, .share = 0.25f};
	const auto needed = rules::WorshippersNeeded(town, true, true);
	EXPECT_EQ(needed.count, 2);
	EXPECT_TRUE(needed.alreadyEnough);
}

TEST(WorshipperRules, DiscipleRaisesTheShareToKeepItself)
{
	// Still wanting more: the share is left alone
	EXPECT_FALSE(rules::ShareAfterDisciple(1, 2, 0, 10).has_value());
	EXPECT_FALSE(rules::ShareAfterDisciple(0, 2, 0, 0).has_value());
	// (needed + 1.4 + on the way + worshipping) / people
	EXPECT_FLOAT_EQ(*rules::ShareAfterDisciple(0, 2, 1, 10), (0.0f + 1.4f + 1.0f + 2.0f) / 10.0f);
	EXPECT_FLOAT_EQ(*rules::ShareAfterDisciple(-1, 2, 1, 10), (-1.0f + 1.4f + 1.0f + 2.0f) / 10.0f);
	EXPECT_FLOAT_EQ(*rules::ShareAfterDisciple(0, 9, 1, 10), 1.0f);
}

TEST(WorshipperRules, CandidatesNearAndHealthyFirst)
{
	const float near = rules::CandidateKey(10.0f, 50.0f, 1.0f);
	const float far = rules::CandidateKey(140.0f, 50.0f, 1.0f);
	const float weak = rules::CandidateKey(10.0f, 50.0f, 0.5f);
	EXPECT_GT(near, far);
	EXPECT_FLOAT_EQ(weak, near * 0.125f);
	// Judged against the town's distance and 100 m more
	EXPECT_FLOAT_EQ(rules::CandidateKey(30.0f, 20.0f, 1.0f), openblack::gutils::GetDistanceModifier(30.0f, 120.0f));
}

TEST(WorshipperRules, CandidateOrderIsStable)
{
	const std::array<float, 5> keys {0.5f, 0.9f, 0.5f, 0.1f, 0.9f};
	EXPECT_EQ(rules::CandidateOrder(keys, true), (std::vector<size_t> {1, 4, 0, 2, 3}));
	EXPECT_EQ(rules::CandidateOrder(keys, false), (std::vector<size_t> {3, 0, 2, 1, 4}));
}

TEST(WorshipperRules, LowLifeStaysUnlessIgnored)
{
	EXPECT_TRUE(rules::LifeAllowsWorship(true, 0.1f, 0.3f));
	EXPECT_FALSE(rules::LifeAllowsWorship(false, 0.3f, 0.3f));
	EXPECT_TRUE(rules::LifeAllowsWorship(false, 0.31f, 0.3f));
}

TEST(WorshipperRules, FarSitesNeedAStone)
{
	EXPECT_TRUE(rules::CanReachSite(500.0f, 500.0f, true, false));
	EXPECT_FALSE(rules::CanReachSite(501.0f, 500.0f, true, false));
	EXPECT_TRUE(rules::CanReachSite(501.0f, 500.0f, true, true));
	EXPECT_TRUE(rules::CanReachSite(900.0f, 500.0f, false, false));
}

TEST(WorshipperRules, ArrivingDancesUntilTheDanceIsFull)
{
	EXPECT_EQ(rules::OnReachingGate(10.0f, 0, 20), rules::Arrival::WalkOn);
	EXPECT_EQ(rules::OnReachingGate(9.9f, 19, 20), rules::Arrival::Dance);
	EXPECT_EQ(rules::OnReachingGate(0.0f, 20, 20), rules::Arrival::Hide);
}

TEST(WorshipperRules, ChantingCostsLife)
{
	EXPECT_FLOAT_EQ(rules::ChantDamage(3.0f, 5e-6f), 1.5e-5f);
	EXPECT_FLOAT_EQ(rules::ChantDamage(0.0f, 5e-6f), 0.0f);
}

TEST(WorshipperRules, HungryWorshippersEatAMealFromThePot)
{
	EXPECT_TRUE(rules::EatsAtSite(0.4f, 0.5f, 30, 30));
	EXPECT_FALSE(rules::EatsAtSite(0.5f, 0.5f, 300, 30));
	EXPECT_FALSE(rules::EatsAtSite(0.1f, 0.5f, 29, 30));
}

TEST(WorshipperRules, GoingBack)
{
	EXPECT_EQ(rules::CheckGoingBack(false, 5, true), rules::GoingBack::Decide);
	EXPECT_EQ(rules::CheckGoingBack(true, -1, true), rules::GoingBack::GoHome);
	EXPECT_EQ(rules::CheckGoingBack(true, -1, false), rules::GoingBack::Stay);
	EXPECT_EQ(rules::CheckGoingBack(true, 0, true), rules::GoingBack::Stay);
}

TEST(WorshipperRules, SiteFood)
{
	const std::array<float, 2> bellies {0.5f, 1.0f};
	const std::array<int32_t, 2> dinners {30, 30};
	EXPECT_FLOAT_EQ(rules::DancersFoodNeed(bellies, dinners), 15.0f);
	EXPECT_EQ(rules::FoodNeededFromTown(15.9f, 5, 2), 8);
	EXPECT_EQ(rules::FoodNeededFromTown(15.0f, 10, 10), 0);
	// Food on its way can be below none, raising the want
	EXPECT_EQ(rules::FoodNeededFromTown(15.0f, 0, -5), 20);
	EXPECT_EQ(rules::FoodSpareForWorship(std::nullopt, 0.0f), 0);
	EXPECT_EQ(rules::FoodSpareForWorship(100u, 30.5f), 69);
	EXPECT_EQ(rules::FoodSpareForWorship(10u, 30.0f), 0);
	EXPECT_EQ(rules::FoodForSiteIfEnough(0, 0), 0);
	EXPECT_EQ(rules::FoodForSiteIfEnough(20, 20), 20);
	EXPECT_EQ(rules::FoodForSiteIfEnough(20, 19), 0);
}
