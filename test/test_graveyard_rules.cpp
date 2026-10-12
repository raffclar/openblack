/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <initializer_list>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/GraveyardRules.h"

using namespace openblack::graveyard;
using openblack::AbodeType;

TEST(GraveyardRules, EachDeathCountsOneMoreBurialUntilTheGraveyardIsFull)
{
	EXPECT_EQ(BurialsAfterDeath(0), 1u);
	EXPECT_EQ(BurialsAfterDeath(1), 2u);
	EXPECT_EQ(BurialsAfterDeath(49), 50u);
	EXPECT_EQ(BurialsAfterDeath(50), 50u);
	EXPECT_EQ(BurialsAfterDeath(k_MostBurials), k_MostBurials);
}

TEST(GraveyardRules, ANewGraveyardShowsNoGraveBeforeItsFirstBurialAndOneAfter)
{
	EXPECT_EQ(GravesShown(0), 0u);
	for (uint32_t burials = 1; burials <= 11; ++burials)
	{
		EXPECT_EQ(GravesShown(burials), 1u) << burials;
	}
}

TEST(GraveyardRules, AGraveAppearsForEachWholeGraveTheBurialsMakeUp)
{
	// The burials at which each grave from the second to the ninth first shows
	const std::array<uint32_t, 8> firstShown {12, 17, 23, 28, 34, 39, 45, 50};
	for (size_t i = 0; i < firstShown.size(); ++i)
	{
		const auto graves = static_cast<uint32_t>(i + 2);
		EXPECT_EQ(GravesShown(firstShown.at(i)), graves) << firstShown.at(i);
		EXPECT_EQ(GravesShown(firstShown.at(i) - 1), graves - 1) << firstShown.at(i) - 1;
	}
}

TEST(GraveyardRules, AFullGraveyardShowsNineGravesAndNeverItsScaffold)
{
	EXPECT_EQ(GravesShown(k_MostBurials), 9u);
	uint32_t burials = 0;
	for (int death = 0; death < 200; ++death)
	{
		burials = BurialsAfterDeath(burials);
		EXPECT_LE(GravesShown(burials), 9u);
	}
}

TEST(GraveyardRules, ABodyLiesLessLongWhenItsTownHadAWorkingGraveyard)
{
	EXPECT_EQ(CorpseTurns(true, 120, 600), 120u);
	EXPECT_EQ(CorpseTurns(false, 120, 600), 600u);
}

TEST(GraveyardRules, TheFirstOtherWorkingGraveyardTakesOverFromTheTownsOwn)
{
	const std::vector<OtherBuilding> others {
	    {.working = false, .takesOver = true}, {.working = true, .takesOver = true}, {.working = true, .takesOver = true}};
	const auto takesOver = GraveyardAfterLoss(true, others);
	ASSERT_TRUE(takesOver.has_value());
	EXPECT_EQ(*takesOver, 1u);
}

TEST(GraveyardRules, AnyWorkingCivicBuildingButTheWonderTakesOverAsTheOriginalDoes)
{
	EXPECT_TRUE(TakesOverAsGraveyard(AbodeType::Graveyard));
	for (const auto civic :
	     {AbodeType::Totem, AbodeType::StoragePit, AbodeType::Creche, AbodeType::Workshop, AbodeType::TownCentre,
	      AbodeType::Citadel, AbodeType::FootballPitch, AbodeType::SpellDispenser, AbodeType::Field, AbodeType::Civic})
	{
		EXPECT_TRUE(TakesOverAsGraveyard(civic)) << static_cast<uint32_t>(civic);
	}
	for (const auto other : {AbodeType::Wonder, AbodeType::General, AbodeType::LivingQuarters, AbodeType::Windmill})
	{
		EXPECT_FALSE(TakesOverAsGraveyard(other)) << static_cast<uint32_t>(other);
	}
	// A working field newer than the next graveyard is the one taking over
	const std::vector<OtherBuilding> others {{.working = true, .takesOver = false},
	                                         {.working = true, .takesOver = TakesOverAsGraveyard(AbodeType::Field)},
	                                         {.working = true, .takesOver = true}};
	const auto takesOver = GraveyardAfterLoss(true, others);
	ASSERT_TRUE(takesOver.has_value());
	EXPECT_EQ(*takesOver, 1u);
}

TEST(GraveyardRules, NoOtherWorkingGraveyardLeavesTheTownWithoutOne)
{
	const std::vector<OtherBuilding> others {{.working = false, .takesOver = true}, {.working = true, .takesOver = false}};
	EXPECT_FALSE(GraveyardAfterLoss(true, others).has_value());
	EXPECT_FALSE(GraveyardAfterLoss(true, {}).has_value());
}

TEST(GraveyardRules, LosingASpareGraveyardLeavesTheTownWithoutOneAsInTheOriginal)
{
	const std::vector<OtherBuilding> others {{.working = true, .takesOver = true}};
	EXPECT_FALSE(GraveyardAfterLoss(false, others).has_value());
}

TEST(GraveyardRules, AStandInCountsABurialOnItsOwnValueAsTheOriginalDoes)
{
	EXPECT_EQ(StandInCountOf(AbodeType::Workshop), StandInCount::ScaffoldTurns);
	EXPECT_EQ(StandInCountOf(AbodeType::Totem), StandInCount::Kept);
	EXPECT_EQ(StandInCountOf(AbodeType::SpellDispenser), StandInCount::DispenserTurns);
	// These keep a link to another object there, or (the crèche) nothing that can be known
	for (const auto other : {AbodeType::TownCentre, AbodeType::StoragePit, AbodeType::Creche, AbodeType::Field,
	                         AbodeType::Citadel, AbodeType::FootballPitch})
	{
		EXPECT_EQ(StandInCountOf(other), StandInCount::None) << static_cast<uint32_t>(other);
	}
}

TEST(GraveyardRules, ABurialOnARestingStandInShowsItsScaffold)
{
	const auto burial = BurialOnStandIn(0);
	ASSERT_TRUE(burial.has_value());
	EXPECT_EQ(burial->count, 1u);
	EXPECT_EQ(burial->partsShown, 1u);
}

TEST(GraveyardRules, AStandInsCountGrowsAsAGraveyardsUntilFull)
{
	const auto burial = BurialOnStandIn(11);
	ASSERT_TRUE(burial.has_value());
	EXPECT_EQ(burial->count, 12u);
	EXPECT_EQ(burial->partsShown, 2u);
	const auto last = BurialOnStandIn(k_MostBurials - 1);
	ASSERT_TRUE(last.has_value());
	EXPECT_EQ(last->count, k_MostBurials);
	EXPECT_EQ(last->partsShown, 9u);
	EXPECT_FALSE(BurialOnStandIn(k_MostBurials).has_value());
	EXPECT_FALSE(BurialOnStandIn(1000).has_value());
}

TEST(GraveyardRules, ASkeletonWearsTheSkeletonAtEveryDistance)
{
	const std::array own {1, 2, 3};
	EXPECT_EQ(ModelsWorn(true, 9, own), (std::array {9, 9, 9}));
	EXPECT_EQ(ModelsWorn(false, 9, own), own);
}
