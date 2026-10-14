/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureMeals.h"
#include "Creature/CreaturePlanActions.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace plan_actions = openblack::creature_plan_actions;
namespace meals = openblack::creature_meals;

namespace
{
uint32_t EatFromStoresFirst(uint32_t /*range*/)
{
	return 0;
}
} // namespace

// A hungry creature walks up to the store's food pile, faces it, acts on itself from then on, takes a handful of wheat
// from the pile and eats it
TEST(CreatureEatFromStores, EatingFromAStoragePitTakesAHandfulFromItsFoodPile)
{
	const auto* eat = plan_actions::For("EatFromStoragePit");
	ASSERT_NE(eat, nullptr);
	EXPECT_EQ(eat->target, plan_actions::Target::StoragePitWithFood);
	EXPECT_EQ(eat->activity, Activity::Eat);
	plan_actions::Situation situation {.height = 10.0f};
	situation.storeFood = plan_actions::Situation::FoodPile {.pile = 21, .point = {5.0f, 7.0f}};
	const auto agenda = plan_actions::Agenda(*eat, 9, {50.0f, 60.0f}, situation, EatFromStoresFirst);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 5);
	EXPECT_EQ((*agenda)[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ((*agenda)[0].movement.point, glm::vec2(5.0f, 7.0f));
	EXPECT_FLOAT_EQ((*agenda)[0].movement.maxDistance, 14.0f);
	EXPECT_EQ((*agenda)[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ((*agenda)[1].movement.object, 21u);
	EXPECT_FLOAT_EQ((*agenda)[1].seconds, 0.1f);
	EXPECT_EQ((*agenda)[2].kind, Step::Kind::ActOnItself);
	EXPECT_EQ((*agenda)[3].order.kind, ObjectOrder::Kind::TakeFromPile);
	EXPECT_EQ((*agenda)[3].order.handful, ObjectOrder::Handful::Wheat);
	EXPECT_EQ((*agenda)[3].order.object, 21u);
	EXPECT_EQ((*agenda)[3].effect, Effect::TookFromPile);
	EXPECT_EQ((*agenda)[4].order.kind, ObjectOrder::Kind::Eat);
	EXPECT_EQ((*agenda)[4].effect, Effect::Eat);
	// A pit with no food pile gives no agenda
	EXPECT_FALSE(plan_actions::Agenda(*eat, 9, {50.0f, 60.0f}, {.height = 10.0f}, EatFromStoresFirst).has_value());
}

// The game's data names eating from a field "EatFromContainer": the creature walks to within its height of the field
TEST(CreatureEatFromStores, EatingFromAFieldTakesAHandfulOfWheat)
{
	const auto* eat = plan_actions::For("EatFromContainer");
	ASSERT_NE(eat, nullptr);
	EXPECT_EQ(eat->target, plan_actions::Target::FieldWithFood);
	const auto agenda = plan_actions::Agenda(*eat, 9, {50.0f, 60.0f}, {.height = 10.0f}, EatFromStoresFirst);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 5);
	EXPECT_EQ((*agenda)[0].movement.point, glm::vec2(50.0f, 60.0f));
	EXPECT_FLOAT_EQ((*agenda)[0].movement.maxDistance, 10.0f);
	EXPECT_EQ((*agenda)[1].movement.object, 9u);
	EXPECT_EQ((*agenda)[2].kind, Step::Kind::ActOnItself);
	EXPECT_EQ((*agenda)[3].order.object, 9u);
	EXPECT_EQ((*agenda)[3].order.handful, ObjectOrder::Handful::Wheat);
	EXPECT_EQ((*agenda)[4].effect, Effect::Eat);
	// With something it can eat in its hand, it does not go to the field
	plan_actions::Situation holding {.height = 10.0f};
	holding.holdingEatable = true;
	EXPECT_FALSE(plan_actions::Agenda(*eat, 9, {50.0f, 60.0f}, holding, EatFromStoresFirst).has_value());
}

// From a tree it goes to within a fifth more than both radii and takes fruit; it keeps acting on the tree
TEST(CreatureEatFromStores, EatingFromATreeTakesAHandfulOfFruit)
{
	const auto* eat = plan_actions::For("EatFromTree");
	ASSERT_NE(eat, nullptr);
	EXPECT_EQ(eat->target, plan_actions::Target::TreeBigEnough);
	const auto agenda =
	    plan_actions::Agenda(*eat, 9, {50.0f, 60.0f}, {.radius = 6.0f, .thingRadius = 4.0f}, EatFromStoresFirst);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 4);
	EXPECT_FLOAT_EQ((*agenda)[0].movement.maxDistance, 12.0f);
	EXPECT_EQ((*agenda)[1].movement.object, 9u);
	EXPECT_EQ((*agenda)[2].order.kind, ObjectOrder::Kind::TakeFromPile);
	EXPECT_EQ((*agenda)[2].order.handful, ObjectOrder::Handful::Fruit);
	EXPECT_EQ((*agenda)[3].order.kind, ObjectOrder::Kind::Eat);
}

// A field counts once it is sown and its crop holds food, ripe or not
TEST(CreatureEatFromStores, AFieldHasFoodWhenSownWithFoodInItsCrop)
{
	EXPECT_TRUE(meals::FieldHasFood(1.0f, 10.0f));
	EXPECT_FALSE(meals::FieldHasFood(0.0f, 10.0f));
	EXPECT_FALSE(meals::FieldHasFood(1.0f, 0.0f));
}

// A storage pit serves a creature it is not too wide for, while its food pile holds food
TEST(CreatureEatFromStores, AStoragePitServesACreatureBigEnoughForIt)
{
	EXPECT_TRUE(meals::StoragePitServes(15.0f, 1.0f, 10));
	EXPECT_FALSE(meals::StoragePitServes(15.1f, 1.0f, 10));
	EXPECT_FALSE(meals::StoragePitServes(10.0f, 1.0f, 0));
	EXPECT_TRUE(meals::StoragePitServes(7.5f, 0.5f, 1));
}

// A tree serves a creature it is tall enough for: fifteen times its size or more
TEST(CreatureEatFromStores, ATreeServesACreatureSmallEnoughForIt)
{
	EXPECT_TRUE(meals::TreeBigEnough(15.0f, 1.0f));
	EXPECT_FALSE(meals::TreeBigEnough(14.9f, 1.0f));
	EXPECT_TRUE(meals::TreeBigEnough(20.0f, 0.5f));
}

// A handful holds a thousand of food a unit of size, cut to a whole number, but no more than the thing has in store. A
// field and a tree have no store of food, so their handful holds none.
TEST(CreatureEatFromStores, AHandfulHoldsNoMoreThanTheThingHasInStore)
{
	EXPECT_EQ(meals::HandfulAmount(1.0f, 5000), 1000u);
	EXPECT_EQ(meals::HandfulAmount(0.4567f, 5000), 456u);
	EXPECT_EQ(meals::HandfulAmount(2.0f, 300), 300u);
	EXPECT_EQ(meals::HandfulAmount(1.0f, 0), 0u);
}

// A meal kept down takes its energy off the hunger, which stays from 0 to 1
TEST(CreatureEatFromStores, AMealTakesItsEnergyOffTheHunger)
{
	EXPECT_FLOAT_EQ(meals::HungerAfterMeal(0.8f, 0.3f), 0.5f);
	EXPECT_FLOAT_EQ(meals::HungerAfterMeal(0.2f, 0.5f), 0.0f);
	EXPECT_FLOAT_EQ(meals::HungerAfterMeal(1.5f, 0.1f), 1.0f);
}

// Eating a villager, an animal or a creature counts as a kill, kept down or not; only a meal kept down counts by sort
TEST(CreatureEatFromStores, MealsAreCountedByWhatWasEaten)
{
	meals::Tallies tallies;
	meals::CountMeal(tallies, meals::k_VillagerSort, true);
	meals::CountMeal(tallies, meals::k_AnimalSort, false);
	meals::CountMeal(tallies, meals::k_CreatureSort, true);
	meals::CountMeal(tallies, 13, true);
	meals::CountMeal(tallies, 13, false);
	meals::CountMeal(tallies, std::nullopt, true);
	EXPECT_EQ(tallies.peopleKilled, 1u);
	EXPECT_EQ(tallies.animalsKilled, 1u);
	EXPECT_EQ(tallies.creaturesKilled, 1u);
	EXPECT_EQ(tallies.keptDown[meals::k_VillagerSort], 1u);
	EXPECT_EQ(tallies.keptDown.count(meals::k_AnimalSort), 0u);
	EXPECT_EQ(tallies.keptDown[13], 1u);
}
