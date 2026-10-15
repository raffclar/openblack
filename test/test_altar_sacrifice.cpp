/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/AltarSacrifice.h"

namespace as = openblack::ecs::altar_sacrifice;

namespace
{
as::Offered Villager(float life, float tableValue)
{
	return {.kind = as::Offering::Villager, .life = life, .tableValue = tableValue, .siteBuilt = true};
}
} // namespace

TEST(AltarSacrifice, ValueGoesFromHalfAtNoLifeToAllAtFullLife)
{
	EXPECT_FLOAT_EQ(as::SacrificeValue(1.0f, 40.0f), 40.0f);
	EXPECT_FLOAT_EQ(as::SacrificeValue(0.0f, 40.0f), 20.0f);
	EXPECT_FLOAT_EQ(as::SacrificeValue(0.5f, 40.0f), 30.0f);
}

TEST(AltarSacrifice, AVillagerDiesAndIsWorthMoreAsAChild)
{
	const auto adult = as::Judge(Villager(1.0f, 40.0f));
	ASSERT_TRUE(adult.has_value());
	EXPECT_FLOAT_EQ(adult->prayer, 40.0f);
	EXPECT_TRUE(adult->deed);
	EXPECT_TRUE(adult->villagerDies);
	EXPECT_TRUE(adult->light);
	EXPECT_TRUE(adult->number);
	EXPECT_FALSE(adult->ghost);
	EXPECT_FALSE(adult->animalCounts);

	auto offered = Villager(0.5f, 40.0f);
	offered.child = true;
	const auto child = as::Judge(offered);
	ASSERT_TRUE(child.has_value());
	EXPECT_FLOAT_EQ(child->prayer, 30.0f * 1.25f);
}

TEST(AltarSacrifice, AnAltarRefusesAVillagerUntilItsSiteIsBuiltAndAnIndestructibleOne)
{
	auto offered = Villager(1.0f, 40.0f);
	offered.siteBuilt = false;
	EXPECT_FALSE(as::Judge(offered).has_value());
	offered.siteBuilt = true;
	offered.indestructible = true;
	EXPECT_FALSE(as::Judge(offered).has_value());
}

TEST(AltarSacrifice, AnAnimalIsWeightedByItsLifeAndTakenWhateverTheSite)
{
	const auto animal = as::Judge(
	    {.kind = as::Offering::Animal, .life = 0.5f, .tableValue = 40.0f, .indestructible = true, .siteBuilt = false});
	ASSERT_TRUE(animal.has_value());
	EXPECT_FLOAT_EQ(animal->prayer, 30.0f * 0.5f);
	EXPECT_TRUE(animal->deed);
	EXPECT_TRUE(animal->animalCounts);
	EXPECT_TRUE(animal->number);
	EXPECT_FALSE(animal->villagerDies);
	EXPECT_FALSE(animal->light);
	EXPECT_FALSE(animal->ghost);
}

TEST(AltarSacrifice, TreesFadeAsGhostsAndAreNoDeed)
{
	const auto tree = as::Judge({.kind = as::Offering::Tree, .life = 1.0f, .tableValue = 10.0f});
	ASSERT_TRUE(tree.has_value());
	EXPECT_FLOAT_EQ(tree->prayer, 10.0f);
	EXPECT_TRUE(tree->ghost);
	EXPECT_TRUE(tree->number);
	EXPECT_FALSE(tree->deed);

	EXPECT_FALSE(as::Judge({.kind = as::Offering::Tree, .life = 1.0f, .tableValue = 10.0f, .indestructible = true}));

	// A dead tree is taken even when indestructible, and shows no number
	const auto dead = as::Judge({.kind = as::Offering::DeadTree, .life = 0.0f, .tableValue = 10.0f, .indestructible = true});
	ASSERT_TRUE(dead.has_value());
	EXPECT_FLOAT_EQ(dead->prayer, 0.0f);
	EXPECT_TRUE(dead->ghost);
	EXPECT_FALSE(dead->number);
	EXPECT_FALSE(dead->deed);
}

TEST(AltarSacrifice, AnAnimalMovesItsSacrificersAlignmentHalfAsMuch)
{
	EXPECT_FLOAT_EQ(as::AnimalAlignmentChange(-1.0f), -0.5f);
}

TEST(AltarSacrifice, MenAndWomenCryButAChildDoesNot)
{
	const auto man = as::CryOf(false, false);
	ASSERT_TRUE(man.has_value());
	EXPECT_EQ(man->first, 221u);
	EXPECT_EQ(man->count, 8u);
	const auto woman = as::CryOf(false, true);
	ASSERT_TRUE(woman.has_value());
	EXPECT_EQ(woman->first, 536u);
	EXPECT_EQ(woman->count, 6u);
	EXPECT_FALSE(as::CryOf(true, false).has_value());
	EXPECT_FALSE(as::CryOf(true, true).has_value());
}
