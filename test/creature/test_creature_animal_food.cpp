/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Animals as the creatures' food and targets: their worth to eat, what a creature knows of one, whether it may be picked
// up, and the mind's scans finding them

#define LOCATOR_IMPLEMENTATIONS

#include <gtest/gtest.h>

#include "Creature/CreatureDecisionTree.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Transform.h"
#include "ECS/Systems/Implementations/CreatureMindSystemDetail.h"
#include "ECS/Systems/Implementations/CreatureObjectActionSystem.h"
#include "creature/CreatureSystemWorld.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreatureObjectActionSystem;
namespace mind_detail = openblack::ecs::systems::mind_detail;
using creature_plan_actions::Target;

namespace
{
class CreatureAnimalFoodTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto& cow = _world.Info().animal.at(static_cast<size_t>(AnimalInfo::Cow));
		cow.foodValue = 30.0f;
		cow.playerCanPickUp = 1;
		_world.Info().animal.at(static_cast<size_t>(AnimalInfo::Lion)).playerCanPickUp = 0;
		// the mind finds food through the creatures' hands
		_hands = &static_cast<CreatureObjectActionSystem&>(
		    Locator::creatureObjectActionSystem::emplace<CreatureObjectActionSystem>());
	}

	static entt::entity MakeAnimal(AnimalInfo type, glm::vec3 position, int32_t player = -1)
	{
		auto& registry = test::creature_world::World::Registry();
		const auto entity = registry.Create();
		registry.Assign<ecs::components::Animal>(entity, ecs::components::Animal {.type = type, .age = 0, .player = player});
		registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}

	test::creature_world::World _world;
	const test::RestoreService<Locator::creatureObjectActionSystem> _restoreHands;
	CreatureObjectActionSystem* _hands {nullptr};
};
} // namespace

TEST_F(CreatureAnimalFoodTest, AnAnimalIsWorthItsFoodValue)
{
	const auto cow = MakeAnimal(AnimalInfo::Cow, glm::vec3(0.0f));
	EXPECT_EQ(_hands->FoodValueOf(cow), std::optional(30.0f));
	// a kind worth nothing to eat is no food
	EXPECT_FALSE(_hands->FoodValueOf(MakeAnimal(AnimalInfo::Wolf, glm::vec3(0.0f))).has_value());
}

TEST_F(CreatureAnimalFoodTest, OnlyTheAnimalsAHandMayHoldArePickedUp)
{
	EXPECT_TRUE(_hands->CanPickUp(MakeAnimal(AnimalInfo::Cow, glm::vec3(0.0f))));
	EXPECT_FALSE(_hands->CanPickUp(MakeAnimal(AnimalInfo::Lion, glm::vec3(0.0f))));
}

TEST_F(CreatureAnimalFoodTest, WhatACreatureKnowsOfAnAnimal)
{
	const auto self = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_ONE);
	const auto wild = MakeAnimal(AnimalInfo::Cow, glm::vec3(0.0f));
	const auto mine = MakeAnimal(AnimalInfo::Cow, glm::vec3(0.0f), 0);
	const auto theirs = MakeAnimal(AnimalInfo::Cow, glm::vec3(0.0f), 2);
	const auto& registry = std::as_const(test::creature_world::World::Registry());
	using creature_tree::Attribute;
	const auto belief = mind_detail::BeliefOf(registry, wild, self);
	ASSERT_TRUE(belief.has_value());
	EXPECT_EQ(belief->type, creature_tree::belief_types::k_Animal);
	EXPECT_EQ(belief->Value(Attribute::Animate), 1u);
	EXPECT_EQ(belief->Value(Attribute::Life), 1u);
	// nobody's, the creature's own player's, another's
	EXPECT_EQ(belief->Value(Attribute::Allegiance), 1u);
	EXPECT_EQ(mind_detail::BeliefOf(registry, mine, self)->Value(Attribute::Allegiance), 0u);
	EXPECT_EQ(mind_detail::BeliefOf(registry, theirs, self)->Value(Attribute::Allegiance), 2u);
	EXPECT_EQ(mind_detail::BeliefOf(registry, theirs, self)->Value(Attribute::PlayerNumber), 2u);
}

TEST_F(CreatureAnimalFoodTest, AnAnimalIsLiveFoodAndALiving)
{
	const auto self = test::creature_world::World::MakeCreature(glm::vec3(0.0f));
	const auto cow = MakeAnimal(AnimalInfo::Cow, glm::vec3(10.0f, 0.0f, 0.0f));
	const auto wolf = MakeAnimal(AnimalInfo::Wolf, glm::vec3(5.0f, 0.0f, 0.0f));
	const auto& registry = std::as_const(test::creature_world::World::Registry());
	EXPECT_TRUE(mind_detail::Accepts(registry, Target::LiveFood, cow, self));
	EXPECT_FALSE(mind_detail::Accepts(registry, Target::LiveFood, wolf, self));
	EXPECT_TRUE(mind_detail::Accepts(registry, Target::Living, wolf, self));
}

TEST_F(CreatureAnimalFoodTest, TheScansFindAnimalsToEat)
{
	const auto self = test::creature_world::World::MakeCreature(glm::vec3(0.0f));
	const auto cow = MakeAnimal(AnimalInfo::Cow, glm::vec3(10.0f, 0.0f, 0.0f));
	MakeAnimal(AnimalInfo::Wolf, glm::vec3(5.0f, 0.0f, 0.0f));
	const auto& registry = std::as_const(test::creature_world::World::Registry());
	const auto found = mind_detail::Gather(registry, Target::Food, self, glm::vec2(0.0f));
	ASSERT_EQ(found.size(), 1u);
	EXPECT_EQ(found.front().entity, cow);
	const auto nearest = mind_detail::NearestFood(registry, glm::vec2(0.0f), {});
	ASSERT_TRUE(nearest.has_value());
	EXPECT_EQ(nearest->first, cow);
	EXPECT_EQ(nearest->second, glm::vec2(10.0f, 0.0f));
}
