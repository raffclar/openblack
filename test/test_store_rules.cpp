/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/StoreRules.h"

using namespace openblack::ecs;

TEST(StoreRules, ATreesWoodScalesOnceWithItsSizeAndWithItsLife)
{
	EXPECT_EQ(store_rules::TreeWood(1.0f, 1.0f, 100, 2.0f, 1.0f), 200u);
	EXPECT_EQ(store_rules::TreeWood(0.5f, 1.0f, 100, 2.0f, 1.0f), 100u);
	// A forest miracle's tree and the land's balance multiply it, and it is truncated
	EXPECT_EQ(store_rules::TreeWood(1.0f, 1.5f, 100, 1.0f, 0.999f), 149u);
}

TEST(StoreRules, ADeadTreesWoodIgnoresItsLife)
{
	EXPECT_EQ(store_rules::DeadTreeWood(2.0f, 100, 1.0f), 200u);
}

TEST(StoreRules, ADeadTreesWoodMultipliesTheWorthFirstAndTheScaleLast)
{
	// 100 x 0.7 is 70 as a float, and 70 x 2.1 is 147.0; the other order gives 146.99998
	EXPECT_EQ(store_rules::DeadTreeWood(2.1f, 100, 0.7f), 147u);
}

TEST(StoreRules, AFencesWoodGrowsWithTheCubeOfItsSize)
{
	EXPECT_EQ(store_rules::FenceWood(1.0f, 10, 2.0f), 80u);
	EXPECT_EQ(store_rules::FenceWood(0.5f, 10, 2.0f), 40u);
}

TEST(StoreRules, OnlyMeatAndVegetableAnimalsAreFood)
{
	EXPECT_EQ(store_rules::AnimalFood(300.0f, 1), 300u);
	EXPECT_EQ(store_rules::AnimalFood(300.0f, 2), 300u);
	EXPECT_EQ(store_rules::AnimalFood(300.0f, 4), 0u);
}

TEST(StoreRules, GivingBackWhatWasTakenLatelyCountsForLess)
{
	EXPECT_FLOAT_EQ(store_rules::LastTakenModifier(std::nullopt, 100, 50), 1.0f);
	EXPECT_FLOAT_EQ(store_rules::LastTakenModifier(75, 100, 50), 0.125f);
	EXPECT_FLOAT_EQ(store_rules::LastTakenModifier(0, 100, 50), 1.0f);
}

TEST(StoreRules, APileGivesOnlyWhatIsWithinItsStoresMaximum)
{
	EXPECT_EQ(store_rules::AmountOverMaximum(true, 600, 100), 100);
	EXPECT_EQ(store_rules::AmountOverMaximum(false, 600, 100), 500);
	EXPECT_EQ(store_rules::AskedOfPile(50, 100), 0u);
	EXPECT_EQ(store_rules::AskedOfPile(150, 100), 50u);
	EXPECT_EQ(store_rules::AskedOfPile(150, -10), 150u);
}

TEST(StoreRules, AForestCountsItsWoodWithoutTruncating)
{
	// A small tree worth less than one wood still counts for its forest
	EXPECT_FLOAT_EQ(store_rules::TreeWoodValue(0.5f, 1.0f, 1u, 0.5f, 1.0f), 0.25f);
	EXPECT_EQ(store_rules::TreeWood(0.5f, 1.0f, 1u, 0.5f, 1.0f), 0u);
}

TEST(StoreRules, ABuildingSiteStoresOnlyWood)
{
	EXPECT_TRUE(store_rules::BuildingSiteStores(openblack::ResourceType::Wood));
	EXPECT_TRUE(store_rules::BuildingSiteStores(openblack::ResourceType::Any));
	EXPECT_FALSE(store_rules::BuildingSiteStores(openblack::ResourceType::Food));
	EXPECT_FALSE(store_rules::BuildingSiteStores(openblack::ResourceType::None));
}

TEST(StoreRules, AScaffoldStoresOnlyWoodAndOnlyOnASite)
{
	EXPECT_TRUE(store_rules::ScaffoldStores(openblack::ResourceType::Wood, true));
	EXPECT_FALSE(store_rules::ScaffoldStores(openblack::ResourceType::Wood, false));
	EXPECT_FALSE(store_rules::ScaffoldStores(openblack::ResourceType::Any, true));
	EXPECT_FALSE(store_rules::ScaffoldStores(openblack::ResourceType::Food, true));
}

TEST(StoreRules, ARocksWoodIsItsScaleTimesItsWoodValueCutTowardsZero)
{
	EXPECT_EQ(store_rules::RockWood(1.0f, 200), 200u);
	EXPECT_EQ(store_rules::RockWood(0.7f, 200), 140u);
	// 0.333 x 200 is 66.6, cut to 66
	EXPECT_EQ(store_rules::RockWood(0.333f, 200), 66u);
}

TEST(StoreRules, WoodGivenToARockGrowsItUpToItsLargestScale)
{
	EXPECT_FLOAT_EQ(store_rules::ScaleAfterAddingWood(200, 100, 200, 3.0f), 1.5f);
	EXPECT_FLOAT_EQ(store_rules::ScaleAfterAddingWood(200, 1000, 200, 3.0f), 3.0f);
	// Exactly at the largest scale it stays there
	EXPECT_FLOAT_EQ(store_rules::ScaleAfterAddingWood(200, 400, 200, 3.0f), 3.0f);
}

TEST(StoreRules, WoodTakenFromARockShrinksItAndItsLife)
{
	const auto take = store_rules::TakeFromRock(200, 50, 200, 1.0f, 1.0f);
	EXPECT_EQ(take.taken, 50u);
	ASSERT_TRUE(take.scale.has_value());
	EXPECT_FLOAT_EQ(*take.scale, 0.75f);
	EXPECT_FLOAT_EQ(take.life, 0.75f);
	// The life goes down by the share of the scale that is left, from the scale it had
	const auto half = store_rules::TakeFromRock(400, 200, 200, 0.5f, 2.0f);
	ASSERT_TRUE(half.scale.has_value());
	EXPECT_FLOAT_EQ(*half.scale, 1.0f);
	EXPECT_FLOAT_EQ(half.life, 0.25f);
}

TEST(StoreRules, ARockGivesAllItsWoodAndGoesWhenNoMoreIsLeft)
{
	const auto all = store_rules::TakeFromRock(200, 200, 200, 1.0f, 1.0f);
	EXPECT_EQ(all.taken, 200u);
	EXPECT_FALSE(all.scale.has_value());
	// A rock asked for a resource it does not hold holds none, so it goes and gives nothing
	const auto none = store_rules::TakeFromRock(0, 10, 200, 1.0f, 1.0f);
	EXPECT_EQ(none.taken, 0u);
	EXPECT_FALSE(none.scale.has_value());
}
