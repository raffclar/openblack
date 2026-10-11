/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>

#include <gtest/gtest.h>

#include "ECS/VillagerCheckRules.h"
#include "ECS/VillagerFoodRules.h"

using namespace openblack;
using namespace openblack::ecs::villager_food_rules;
namespace checks = openblack::ecs::villager_check_rules;

namespace
{
/// A villager at rest with the info table's usual numbers
DigestInputs Resting(float food)
{
	return {.turnsSinceChecked = 9,
	        .food = food,
	        .foodPerTurn = 9e-5f,
	        .hungryForFood = 0.5f,
	        .hungerToLife = 0.001f,
	        .speed = 100.0f,
	        .walkingSpeed = 100.0f};
}
} // namespace

TEST(VillagerFoodRules, NothingHappensWhenNoTurnHasPassed)
{
	auto in = Resting(1.0f);
	in.turnsSinceChecked = 0;
	EXPECT_FALSE(Digest(in).has_value());
}

TEST(VillagerFoodRules, ACheckBurnsTheTurnsSinceTheLastTimesTheRate)
{
	const auto result = Digest(Resting(1.0f));
	ASSERT_TRUE(result.has_value());
	EXPECT_FLOAT_EQ(result->food, 1.0f - 9.0f * 9e-5f);
	EXPECT_FALSE(result->hungry);
	EXPECT_FLOAT_EQ(result->lifeLost, 0.0f);
}

TEST(VillagerFoodRules, FoodNeverGoesBelowEmpty)
{
	auto in = Resting(0.0001f);
	const auto result = Digest(in);
	ASSERT_TRUE(result.has_value());
	EXPECT_FLOAT_EQ(result->food, 0.0f);
	EXPECT_TRUE(result->hungry);
}

TEST(VillagerFoodRules, TheJapanesePowerMakesFoodLast)
{
	auto in = Resting(1.0f);
	in.tribalPower = 3.0f;
	EXPECT_FLOAT_EQ(Digest(in)->food, 1.0f - 9.0f * 9e-5f / 3.0f);
}

TEST(VillagerFoodRules, GoingFasterThanItsWalkBurnsMoreOnlyWhileMoving)
{
	auto in = Resting(1.0f);
	in.speed = 200.0f;
	EXPECT_FLOAT_EQ(Digest(in)->food, 1.0f - 9.0f * 9e-5f);
	in.moving = true;
	EXPECT_FLOAT_EQ(Digest(in)->food, 1.0f - 2.0f * 9.0f * 9e-5f);
	// Slower than its walk burns no less
	in.speed = 50.0f;
	EXPECT_FLOAT_EQ(Digest(in)->food, 1.0f - 9.0f * 9e-5f);
}

TEST(VillagerFoodRules, HungryUnderTheMarkAndLosingTheWholeRate)
{
	const auto result = Digest(Resting(0.3f));
	EXPECT_TRUE(result->hungry);
	EXPECT_FLOAT_EQ(result->lifeLost, 0.001f);
	// Right on the mark after the check is not yet hungry here, though it is hungry to its needs
	auto in = Resting(0.5f + 9.0f * 9e-5f);
	EXPECT_FALSE(Digest(in)->hungry);
}

TEST(VillagerFoodRules, PoisonCostsLifeEvenWhenFed)
{
	auto in = Resting(1.0f);
	in.poisoned = true;
	const auto result = Digest(in);
	EXPECT_FALSE(result->hungry);
	EXPECT_FLOAT_EQ(result->lifeLost, 0.001f);
}

TEST(VillagerFoodRules, FullToHungryTakesOverFiveThousandTurnsAtRest)
{
	float food = 1.0f;
	uint32_t checks = 0;
	while (!Digest(Resting(food))->hungry)
	{
		food = Digest(Resting(food))->food;
		++checks;
	}
	// 0.5 / (9 * 9e-5) = 617 checks of nine turns
	EXPECT_NEAR(static_cast<float>(checks), 617.0f, 1.0f);
}

TEST(VillagerFoodRules, HungerTakesItToEatUnlessAWorkingDiscipleWhoOnlyGoesStarving)
{
	int tries = 0;
	const auto fail = [&tries] {
		++tries;
		return false;
	};
	const auto succeed = [&tries] {
		++tries;
		return true;
	};
	EXPECT_TRUE(SeeksFood(true, true, false, 0.4f, 0.25f, succeed));
	EXPECT_EQ(tries, 1);
	tries = 0;
	// A working disciple keeps working while only hungry
	EXPECT_FALSE(SeeksFood(true, true, true, 0.4f, 0.25f, succeed));
	EXPECT_EQ(tries, 0);
	// Starving, it goes
	EXPECT_TRUE(SeeksFood(true, true, true, 0.2f, 0.25f, succeed));
	EXPECT_EQ(tries, 1);
	tries = 0;
	// A failed try is tried again once starving
	EXPECT_FALSE(SeeksFood(true, true, false, 0.2f, 0.25f, fail));
	EXPECT_EQ(tries, 2);
	tries = 0;
	// A state that gives way to neither keeps it
	EXPECT_FALSE(SeeksFood(false, false, false, 0.0f, 0.25f, succeed));
	EXPECT_EQ(tries, 0);
}

TEST(VillagerFoodRules, AMealIsAsMuchOfItsDinnerAsItWantsFood)
{
	EXPECT_EQ(AmountToEat(0.875f, 80, std::nullopt), 70u);
	// A town wanting food fully eats a hair under three tenths less
	EXPECT_EQ(AmountToEat(1.0f, 100, 1.0f), 70u);
	EXPECT_EQ(AmountToEat(1.0f, 100, 2.0f), 70u);
	EXPECT_EQ(AmountToEat(1.0f, 100, -1.0f), 100u);
	EXPECT_FLOAT_EQ(k_TownShortageShare, 0.29999998f);
	EXPECT_LT(k_TownShortageShare, 0.3f);
	EXPECT_EQ(RequiredForMeal(70, 20), 50u);
	EXPECT_EQ(RequiredForMeal(70, 90), 0u);
}

TEST(VillagerFoodRules, AMealComesFromHandsHomePitOrNowhere)
{
	// Enough in its hands: eaten where it is, or at home when inside
	EXPECT_EQ(PlanMeal({.required = 0, .carried = 50}), MealPlan::EatHeld);
	EXPECT_EQ(PlanMeal({.required = 0, .carried = 50, .atHome = true}), MealPlan::EatAtHome);
	// Home has enough with what it carries
	EXPECT_EQ(PlanMeal({.required = 50, .homeFood = 50u}), MealPlan::GoHome);
	EXPECT_EQ(PlanMeal({.required = 50, .atHome = true, .homeFood = 50u}), MealPlan::EatAtHome);
	// What it carries is already off what it needs and counts only once: home must hold the rest
	EXPECT_EQ(PlanMeal({.required = 50, .carried = 20, .homeFood = 30u, .pitFood = 100u}), MealPlan::GoToStoragePit);
	EXPECT_EQ(PlanMeal({.required = 50, .carried = 20, .homeFood = 50u}), MealPlan::GoHome);
	// Else the storage pit
	EXPECT_EQ(PlanMeal({.required = 50, .homeFood = 10u, .pitFood = 60u}), MealPlan::GoToStoragePit);
	// The pit short: what it carries, if anything
	EXPECT_EQ(PlanMeal({.required = 50, .homeFood = 10u, .pitFood = 10u}), MealPlan::None);
	EXPECT_EQ(PlanMeal({.required = 50, .carried = 5, .pitFood = 10u}), MealPlan::EatHeld);
	// No working pit: where its town leaves food, when that is elsewhere
	EXPECT_EQ(PlanMeal({.required = 50, .dropOffElsewhere = true}), MealPlan::GoToDropOff);
	EXPECT_EQ(PlanMeal({.required = 50}), MealPlan::None);
}

TEST(VillagerFoodRules, AMealAtHomeTakesWhatItNeedsOnce)
{
	EXPECT_EQ(TakenFromHome(50, 200), 50u);
	EXPECT_EQ(TakenFromHome(50, 30), 30u);
	EXPECT_EQ(TakenFromHome(0, 30), 0u);
	// Eating what it then holds uses exactly the meal: nothing left over in its hands
	const uint32_t meal = 70;
	const int16_t carried = 20;
	const auto taken = TakenFromHome(RequiredForMeal(meal, carried), 500);
	const auto eaten = EatHeld(meal, static_cast<int16_t>(carried + static_cast<int16_t>(taken)), 0.3f, 1.0f);
	EXPECT_EQ(eaten.eaten, meal);
	EXPECT_EQ(static_cast<int32_t>(carried) + static_cast<int32_t>(taken) - eaten.eaten, 0);
}

TEST(VillagerFoodRules, EatingFillsByTheShareOfTheMealEaten)
{
	auto meal = EatHeld(80, 40, 0.2f, 1.2f);
	EXPECT_EQ(meal.eaten, 40);
	EXPECT_FLOAT_EQ(meal.food, 0.2f + 0.5f * 1.2f);
	meal = EatHeld(80, 100, 0.4f, 1.2f);
	EXPECT_EQ(meal.eaten, 80);
	EXPECT_FLOAT_EQ(meal.food, 1.0f);
	// A meal of nothing leaves the belly as it is
	meal = EatHeld(0, 30, 0.9f, 1.0f);
	EXPECT_EQ(meal.eaten, 0);
	EXPECT_FLOAT_EQ(meal.food, 0.9f);
}

TEST(VillagerFoodRules, AHouseholdsDinnerIsEachOfItsPeoplesDinner)
{
	const std::array<uint32_t, 3> dinners {75, 85, 35};
	EXPECT_EQ(FoodForDinner(dinners), 195u);
	EXPECT_TRUE(IsEnoughForDinner(195, 195));
	EXPECT_FALSE(IsEnoughForDinner(195, 194));
	// The want is counted in 16 bits
	EXPECT_TRUE(IsEnoughForDinner(65536 + 10, 10));
	EXPECT_EQ(GossipTurns(75, 0.0f), 75);
	EXPECT_EQ(GossipTurns(75, 0.55f), 80);
}

TEST(VillagerFoodRules, MagicFoodWearsOffATickEveryTenthTurn)
{
	EXPECT_EQ(TickFoodSpeedUp(255, 20), 254);
	EXPECT_EQ(TickFoodSpeedUp(255, 21), 255);
	EXPECT_EQ(TickFoodSpeedUp(0, 20), 0);
	// 2550 turns from eating to wearing off
	uint8_t ticks = k_FoodSpeedUpTicks;
	uint32_t turns = 0;
	for (uint32_t turn = 1; ticks != 0; ++turn, ++turns)
	{
		ticks = TickFoodSpeedUp(ticks, turn);
	}
	EXPECT_EQ(turns, 2550u);
}

TEST(VillagerFoodRules, StarvingToDeathIsStarvationUnlessFromWorship)
{
	EXPECT_EQ(HungerDeath(false), DeathReason::Starving);
	EXPECT_EQ(HungerDeath(true), DeathReason::Chant);
	EXPECT_TRUE(checks::ChantedLast(VillagerStates::GoHomeFromWorship));
	EXPECT_FALSE(checks::ChantedLast(VillagerStates::GoHome));
	EXPECT_EQ(checks::ExhaustionDeath(false), DeathReason::Exhaustion);
}

TEST(VillagerCheckRules, ChecksComeEveryNineTurns)
{
	EXPECT_FALSE(checks::ChecksDue(8, 8));
	EXPECT_TRUE(checks::ChecksDue(9, 8));
	// Its age at its own turn in eight hundred, once
	EXPECT_TRUE(checks::OldAgeDue(795, 0, 9) == false);
	EXPECT_TRUE(checks::OldAgeDue(800, 0, 9));
	EXPECT_TRUE(checks::OldAgeDue(808, 0, 9));
	EXPECT_FALSE(checks::OldAgeDue(809, 0, 9));
	EXPECT_TRUE(checks::OldAgeDue(10, 790, 9));
}

TEST(VillagerCheckRules, AMovingStatePaysItsOwnCost)
{
	const auto walking =
	    checks::DrainAndCheckStates(VillagerStates::MoveToPos, VillagerStates::ArrivesHome, VillagerStates::ArrivesHome, true);
	EXPECT_EQ(walking.drain, VillagerStates::MoveToPos);
	EXPECT_EQ(walking.checks, VillagerStates::ArrivesHome);
	const auto building =
	    checks::DrainAndCheckStates(VillagerStates::Building, VillagerStates::InvalidState, VillagerStates::Building, false);
	EXPECT_EQ(building.drain, VillagerStates::Building);
	EXPECT_EQ(building.checks, VillagerStates::Building);
}

TEST(VillagerCheckRules, TheWoundedGoHomeUnlessHomeKeptOrHungryForFood)
{
	checks::WoundedInputs in {.life = 0.2f, .stateSendsHome = true};
	EXPECT_TRUE(checks::WoundedGoesHome(in));
	in.atHome = true;
	EXPECT_FALSE(checks::WoundedGoesHome(in));
	in.atHome = false;
	in.stateKeepsWounded = true;
	EXPECT_FALSE(checks::WoundedGoesHome(in));
	in.stateKeepsWounded = false;
	// On its way to food it found, it goes on while hungry
	in.goingForFood = true;
	in.food = 0.4f;
	EXPECT_FALSE(checks::WoundedGoesHome(in));
	in.food = 0.6f;
	EXPECT_TRUE(checks::WoundedGoesHome(in));
	in.life = 0.3f;
	EXPECT_FALSE(checks::WoundedGoesHome(in));
}

TEST(VillagerFoodRules, FoodLyingAboutCatchesAHungryEyeWithRoomToCarry)
{
	FoodInterestInputs in {.desireForFood = 0.875f, .maxCarried = 150, .stateInterest = 1.0f, .distanceModifier = 0.5f};
	// 1 x 0.875 x 1 x 0.5 is past a quarter
	EXPECT_TRUE(IsInterestedInFood(in));
	// Laden, it has no room
	in.carried = 150;
	EXPECT_FALSE(IsInterestedInFood(in));
	in.carried = 0;
	// Its state hardly lets it care
	in.stateInterest = 0.01f;
	EXPECT_FALSE(IsInterestedInFood(in));
	// Sped-up food is twice as tempting
	in.stateInterest = 0.3f;
	EXPECT_FALSE(IsInterestedInFood(in));
	in.speedUp = true;
	EXPECT_TRUE(IsInterestedInFood(in));
	// Not hungry itself, its town's want of food moves it past the town's trigger
	in = {.desireForFood = 0.0f,
	      .maxCarried = 150,
	      .stateInterest = 0.2f,
	      .distanceModifier = 0.5f,
	      .townFoodDesire = 0.2f,
	      .townTrigger = 0.01f};
	EXPECT_TRUE(IsInterestedInFood(in));
	in.townFoodDesire = 0.0f;
	EXPECT_FALSE(IsInterestedInFood(in));
}

TEST(VillagerFoodRules, PickedUpFoodGoesToTheStoreWhenTheTownIsShort)
{
	EXPECT_TRUE(CarriesFoodToStore(1.0f, 150, 150, 0.2f));
	EXPECT_FALSE(CarriesFoodToStore(1.0f, 15, 150, 0.9f));
	EXPECT_FALSE(CarriesFoodToStore(0.0f, 150, 150, 1.0f));
}
