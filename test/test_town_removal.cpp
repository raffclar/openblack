/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Animal.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/TownRemoval.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{

entt::entity MakeTown(Registry& registry, uint32_t id)
{
	const auto entity = registry.Create();
	registry.Assign<Town>(entity).id = id;
	registry.Context().towns.insert({id, entity});
	return entity;
}

entt::entity MakeVillagerOf(Registry& registry, entt::entity town)
{
	const auto entity = registry.Create();
	registry.Assign<Villager>(entity).town = town;
	return entity;
}

} // namespace

TEST(TownRemoval, ATownGoesOnlyWithNoBuildingNoPlanAndNobody)
{
	EXPECT_TRUE(town_removal::GoesWhenBuildingLeaves({.buildings = 0, .plans = 0, .people = 0}));
	EXPECT_FALSE(town_removal::GoesWhenBuildingLeaves({.buildings = 1, .plans = 0, .people = 0}));
	EXPECT_FALSE(town_removal::GoesWhenBuildingLeaves({.buildings = 0, .plans = 1, .people = 0}));
	EXPECT_FALSE(town_removal::GoesWhenBuildingLeaves({.buildings = 0, .plans = 0, .people = 1}));
}

TEST(TownRemoval, TheTownKeepsItsValidBuildingsPlansAndPeople)
{
	Registry registry;
	const auto town = MakeTown(registry, 3);
	const auto other = MakeTown(registry, 4);
	const auto hut = registry.Create();
	const auto gone = registry.Create();
	const auto plan = registry.Create();
	registry.Get<Town>(town).abodes = {hut, gone};
	registry.Get<Town>(town).planned = {plan};
	registry.Destroy(gone);
	MakeVillagerOf(registry, town);
	MakeVillagerOf(registry, town);
	MakeVillagerOf(registry, other);
	const auto left = town_removal::LeftIn(registry, town);
	EXPECT_EQ(left.buildings, 1U);
	EXPECT_EQ(left.plans, 1U);
	EXPECT_EQ(left.people, 2U);
	const auto empty = town_removal::LeftIn(registry, other);
	EXPECT_EQ(empty.buildings, 0U);
	EXPECT_EQ(empty.plans, 0U);
	EXPECT_EQ(empty.people, 1U);
}

TEST(TownRemoval, AGoneBuildingAndAGonePlanDoNotKeepTheTown)
{
	Registry registry;
	const auto town = MakeTown(registry, 1);
	const auto hut = registry.Create();
	const auto plan = registry.Create();
	registry.Get<Town>(town).abodes = {hut};
	registry.Get<Town>(town).planned = {plan};
	registry.Destroy(hut);
	registry.Destroy(plan);
	EXPECT_TRUE(town_removal::GoesWhenBuildingLeaves(town_removal::LeftIn(registry, town)));
}

TEST(TownRemoval, TheDependantsAreTheTownsAnimalsChestsAndFlags)
{
	Registry registry;
	const auto town = MakeTown(registry, 1);
	const auto other = MakeTown(registry, 2);
	const auto cow = registry.Create();
	registry.Assign<Animal>(cow).town = town;
	const auto otherCow = registry.Create();
	registry.Assign<Animal>(otherCow).town = other;
	const auto chest = registry.Create();
	registry.Assign<Reward>(chest).town = town;
	const auto flag = registry.Create();
	registry.Assign<TownDesireFlag>(flag).town = town;
	const auto otherFlag = registry.Create();
	registry.Assign<TownDesireFlag>(otherFlag).town = other;
	const auto dependants = town_removal::DependantsOf(registry, town);
	EXPECT_EQ(dependants.animals, std::vector<entt::entity> {cow});
	EXPECT_EQ(dependants.rewards, std::vector<entt::entity> {chest});
	EXPECT_EQ(dependants.desireFlags, std::vector<entt::entity> {flag});
}

TEST(TownRemoval, TheRegistryForgetsTheTown)
{
	Registry registry;
	const auto town = MakeTown(registry, 1);
	const auto other = MakeTown(registry, 2);
	const auto cow = registry.Create();
	registry.Assign<Animal>(cow).town = town;
	const auto chest = registry.Create();
	registry.Assign<Reward>(chest).town = town;
	const auto otherChest = registry.Create();
	registry.Assign<Reward>(otherChest).town = other;
	const auto flag = registry.Create();
	registry.Assign<TownDesireFlag>(flag).town = town;
	const auto field = registry.Create();
	registry.Assign<Field>(field).town = 1;
	town_removal::Forget(registry, town);
	EXPECT_FALSE(registry.Valid(town));
	EXPECT_FALSE(registry.Context().towns.contains(1));
	EXPECT_EQ(registry.Context().towns.at(2), other);
	// The animal stays, with no town
	ASSERT_TRUE(registry.Valid(cow));
	EXPECT_TRUE(registry.Get<const Animal>(cow).town == entt::null);
	// The town's chest and flag go, another town's chest stays
	EXPECT_FALSE(registry.Valid(chest));
	EXPECT_FALSE(registry.Valid(flag));
	EXPECT_TRUE(registry.Valid(otherChest));
	// The field keeps the number of the town, which names no town now
	ASSERT_TRUE(registry.Valid(field));
	EXPECT_EQ(registry.Get<const Field>(field).town, 1);
}

TEST(TownRemoval, TheListKeepsANewerTownWithTheSameNumber)
{
	Registry registry;
	const auto old = registry.Create();
	registry.Assign<Town>(old).id = 5;
	const auto newer = MakeTown(registry, 5);
	town_removal::Forget(registry, old);
	EXPECT_FALSE(registry.Valid(old));
	EXPECT_EQ(registry.Context().towns.at(5), newer);
}

TEST(TownRemoval, ForgetDoesNothingToAThingThatIsNotATown)
{
	Registry registry;
	const auto thing = registry.Create();
	town_removal::Forget(registry, thing);
	EXPECT_TRUE(registry.Valid(thing));
}
