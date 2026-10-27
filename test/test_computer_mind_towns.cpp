/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindDomains.h"
#include "ECS/ComputerMindRules.h"

using namespace openblack;
using namespace openblack::computer_mind;
using namespace openblack::computer_mind_rules;

TEST(ComputerMindTowns, OwnsItsKindsOfNodes)
{
	EXPECT_EQ(DomainOf(NodeKind::ActionTownWithHighestFoodDesire), MindDomain::Towns);
	EXPECT_EQ(DomainOf(NodeKind::ObjectShallCastFoodSpell), MindDomain::Towns);
	EXPECT_EQ(DomainOf(NodeKind::ObjectGetFoodFromField), MindDomain::Towns);
}

TEST(ComputerMindRules, FieldWishAndChoice)
{
	// A full field at the hand with a full pull is wanted almost fully
	const float full = FieldWish(100.0f, 100.0f, 0.0f, 1.0f);
	EXPECT_FLOAT_EQ(full, gutils::GetDistanceModifier(0.0f, k_FieldReach) * ((100.0f + 0.001f) / (100.0f + 0.001f)));
	// An empty field is wanted little, and a field outside the influence that does not pull is not wanted
	EXPECT_LT(FieldWish(0.0f, 100.0f, 0.0f, 1.0f), 0.001f);
	EXPECT_FLOAT_EQ(FieldWish(100.0f, 100.0f, 0.0f, 0.0f), 0.0f);
	// The first of the largest over a tenth wins; none at a tenth or less
	EXPECT_EQ(MostWantedField(std::vector<float> {0.1f, 0.05f}), std::nullopt);
	EXPECT_EQ(MostWantedField(std::vector<float> {0.2f, 0.5f, 0.5f}), std::optional<size_t> {1});
}

TEST(ComputerMindTowns, OwnsTheKindsOfTheFoodAndWoodOfOtherPlaces)
{
	EXPECT_EQ(DomainOf(NodeKind::ObjectGetFoodFromFishFarm), MindDomain::Towns);
	EXPECT_EQ(DomainOf(NodeKind::ObjectGetFoodFromOtherTownForStoragePit), MindDomain::Towns);
	EXPECT_EQ(DomainOf(NodeKind::ObjectGetFoodFromOtherTownForObject), MindDomain::Towns);
	EXPECT_EQ(DomainOf(NodeKind::ObjectGetWoodFromOtherTown), MindDomain::Towns);
	EXPECT_EQ(DomainOf(NodeKind::ObjectCastFoodSpellOn), MindDomain::Towns);
	EXPECT_EQ(DomainOf(NodeKind::ObjectCastWoodSpellOn), MindDomain::Towns);
}

TEST(ComputerMindRules, FishFarmWishHasAFullShare)
{
	// A farm gives its full share: only the distance and the pull change the wish
	EXPECT_FLOAT_EQ(FishFarmWish(0.0f, 1.0f), gutils::GetDistanceModifier(0.0f, k_FieldReach));
	EXPECT_FLOAT_EQ(FishFarmWish(100.0f, 0.5f), 0.5f * gutils::GetDistanceModifier(100.0f, k_FieldReach));
	EXPECT_FLOAT_EQ(FishFarmWish(100.0f, 0.0f), 0.0f);
	// Any farm that the god wants a little can win, not only one over a tenth
	EXPECT_EQ(MostWanting(std::vector<float> {0.05f, 0.02f}), std::optional<size_t> {0});
}

TEST(ComputerMindRules, FoodShortfallRoundsTowardsZero)
{
	EXPECT_EQ(FoodShortfall(100.7f, 50), 50);
	EXPECT_EQ(FoodShortfall(2.5f, 10), -7);
	EXPECT_EQ(FoodShortfall(0.0f, 0), 0);
	// The food held is exact in the subtraction, which rounds one time
	EXPECT_EQ(FoodShortfall(16777216.0f, 16777217u), -1);
}

TEST(ComputerMindRules, GivingTownNeedsAnAskOverFifty)
{
	const std::vector<GivingTown> towns {{.stock = 5000, .rawDesire = 0.0f, .pull = 1.0f}};
	EXPECT_FLOAT_EQ(OtherTownAsk(0.5f), 500.0f);
	EXPECT_EQ(ChooseGivingTown(towns, 0.05f), std::nullopt);
	const auto choice = ChooseGivingTown(towns, 0.06f);
	ASSERT_TRUE(choice.has_value());
	EXPECT_EQ(choice->index, 0u);
	EXPECT_FLOAT_EQ(choice->value, 1.0f);
}

TEST(ComputerMindRules, GivingTownNeedsMoreThanTheAskInItsPit)
{
	// The god asks for 900: a pit of exactly 900 does not give, a pit of 901 does
	const std::vector<GivingTown> towns {{.stock = 900, .rawDesire = 0.0f, .pull = 1.0f},
	                                     {.stock = 901, .rawDesire = 0.5f, .pull = 1.0f}};
	const auto choice = ChooseGivingTown(towns, 0.9f);
	ASSERT_TRUE(choice.has_value());
	EXPECT_EQ(choice->index, 1u);
	EXPECT_FLOAT_EQ(choice->value, 0.5f);
}

TEST(ComputerMindRules, GivingTownWithTheLeastDesireWins)
{
	// A town whose desire times pull is not under the need does not give. Of the rest the least desire wins, the first of
	// equals. A desire over 1 counts as 1.
	const std::vector<GivingTown> towns {{.stock = 5000, .rawDesire = 1.5f, .pull = 1.0f},
	                                     {.stock = 5000, .rawDesire = 0.3f, .pull = 1.0f},
	                                     {.stock = 5000, .rawDesire = 0.2f, .pull = 1.0f},
	                                     {.stock = 5000, .rawDesire = 0.2f, .pull = 1.0f}};
	const auto choice = ChooseGivingTown(towns, 0.9f);
	ASSERT_TRUE(choice.has_value());
	EXPECT_EQ(choice->index, 2u);
	EXPECT_FLOAT_EQ(choice->value, 0.8f);
	EXPECT_EQ(ChooseGivingTown(std::vector<GivingTown> {{.stock = 5000, .rawDesire = 1.5f, .pull = 1.0f}}, 0.9f), std::nullopt);
}

TEST(ComputerMindRules, ContainerTakeKeepsToWhatTheContainerHas)
{
	EXPECT_EQ(ContainerTake(900, std::nullopt, 3000), 900);
	EXPECT_EQ(ContainerTake(900, 100u, 3000), 800);
	EXPECT_EQ(ContainerTake(900, std::nullopt, 500), 500);
	EXPECT_EQ(ContainerTake(900, 1000u, 3000), -100);
	EXPECT_EQ(ContainerTake(900, std::nullopt, 0), 0);
}

TEST(ComputerMindTowns, OwnsTheWoodOfForests)
{
	EXPECT_EQ(DomainOf(NodeKind::ObjectGetWoodFromForest), MindDomain::Towns);
}

TEST(ComputerMindRules, LoneTreeWishFallsWithDistanceOnly)
{
	const SeenWood near {.forest = false, .kindWood = 200, .distanceToEdge = 0.0f};
	const SeenWood far {.forest = false, .kindWood = 200, .distanceToEdge = 500.0f};
	EXPECT_FLOAT_EQ(SeenWoodWish(near, 100.0f), gutils::DistanceChangeToBelief(0.0f, 100.0f) * 200.0f);
	EXPECT_LT(SeenWoodWish(far, 100.0f), SeenWoodWish(near, 100.0f));
	// The pull of the place does not change the wish of a lone tree
	SeenWood pulled = near;
	pulled.pull = 0.0f;
	EXPECT_FLOAT_EQ(SeenWoodWish(pulled, 100.0f), SeenWoodWish(near, 100.0f));
}

TEST(ComputerMindRules, ForestWishCountsGrownTreesAndThePullTwoTimes)
{
	const SeenWood forest {.forest = true, .grownTrees = 4, .treeWood = 50.0f, .pull = 0.5f, .distanceToEdge = 10.0f};
	const float near = gutils::DistanceChangeToBelief(10.0f, 100.0f);
	EXPECT_FLOAT_EQ(SeenWoodWish(forest, 100.0f), 0.5f * (near * ((50.0f * 0.5f) * 4.0f)));
	SeenWood outside = forest;
	outside.pull = 0.0f;
	EXPECT_EQ(SeenWoodWish(outside, 100.0f), 0.0f);
}

TEST(ComputerMindRules, ChosenTreeWishIsTheNearnessOfTheTree)
{
	EXPECT_FLOAT_EQ(ChosenTreeWish(25.0f, 80.0f), gutils::DistanceChangeToBelief(25.0f, 80.0f));
}
