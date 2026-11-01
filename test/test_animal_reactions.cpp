/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <gtest/gtest.h>

#include "Animals/AnimalRules.h"
#include "InfoConstants.h"

using namespace openblack;

TEST(AnimalReactions, AFreeAnimalTakesUpReactions)
{
	EXPECT_TRUE(animals::AvailableForReaction({}));
}

TEST(AnimalReactions, AnAnimalAScriptControlsTakesUpNone)
{
	// A script's cow stays where it was put, even when a creature walks up to it
	EXPECT_FALSE(animals::AvailableForReaction({.scriptControlled = true}));
}

TEST(AnimalReactions, AnAnimalOutOfTheWorldOrBusyTakesUpNone)
{
	EXPECT_FALSE(animals::AvailableForReaction({.outOfTheWorld = true}));
	EXPECT_FALSE(animals::AvailableForReaction({.busyState = true}));
}

TEST(AnimalReactions, BusyStatesAreTheGamesDancingDyingDeadDrowningDownedEatenAndWaitingForAClip)
{
	for (uint8_t state = 0; state < 60; ++state)
	{
		const bool busy = state == 5 || (state >= 14 && state <= 18) || state == 23;
		EXPECT_EQ(animals::IsBusyForReaction(state), busy) << "state " << static_cast<int>(state);
	}
	// Setting off to die is not yet dying
	EXPECT_FALSE(animals::IsBusyForReaction(13));
}

TEST(AnimalReactions, AfterAReactionAnAnimalGoesIntoTheResumeStateOfItsStateBefore)
{
	// A small table: a script state with the empty state, and a wander state that decides again
	std::array<GAnimalStateTableInfo, 33> table {};
	table.at(4).resumeState = 0;
	table.at(32).resumeState = 43;
	EXPECT_EQ(animals::StateAfterReacting(32, table), 43);
	EXPECT_EQ(animals::StateAfterReacting(4, table), 0);
}

TEST(AnimalReactions, AStateThatIsNotInTheTableGivesTheEmptyState)
{
	std::array<GAnimalStateTableInfo, 4> table {};
	table.at(3).resumeState = 43;
	EXPECT_EQ(animals::StateAfterReacting(4, table), 0);
}

TEST(AnimalReactions, BatsVulturesAndTheBigHuntersFrightenACreature)
{
	for (const auto type : {AnimalInfo::Bat, AnimalInfo::SpellBat, AnimalInfo::Vulture, AnimalInfo::Lion, AnimalInfo::Tiger,
	                        AnimalInfo::Leopard, AnimalInfo::Wolf, AnimalInfo::SpellWolf})
	{
		EXPECT_TRUE(animals::FrightensCreatures(type)) << static_cast<int>(type);
	}
	// The lion and the cow of the puzzles and the farm animals do not frighten a creature
	for (const auto type :
	     {AnimalInfo::PuzzleLion, AnimalInfo::PuzzleCow, AnimalInfo::Sheep, AnimalInfo::Cow, AnimalInfo::Horse})
	{
		EXPECT_FALSE(animals::FrightensCreatures(type)) << static_cast<int>(type);
	}
}

TEST(AnimalReactions, AHungryAnimalIsInterestedInFoodOfAKindThatItEats)
{
	const animals::FoodInterest hungry {
	    .available = true, .hungerLimit = 100, .hunger = 100, .eats = FoodType::VegetableGraze, .food = FoodType::Vegetable};
	EXPECT_TRUE(animals::InterestedInFood(hungry));
	auto notHungry = hungry;
	notHungry.hunger = 99;
	EXPECT_FALSE(animals::InterestedInFood(notHungry));
	auto otherFood = hungry;
	otherFood.food = FoodType::Meat;
	EXPECT_FALSE(animals::InterestedInFood(otherFood));
}

TEST(AnimalReactions, AnAnimalWithNoHungerLimitOrFoodInThePhysicsIsNotInterested)
{
	const animals::FoodInterest hungry {
	    .available = true, .hungerLimit = 100, .hunger = 500, .eats = FoodType::Any, .food = FoodType::Meat};
	auto noLimit = hungry;
	noLimit.hungerLimit = 0;
	EXPECT_FALSE(animals::InterestedInFood(noLimit));
	auto flying = hungry;
	flying.inPhysics = true;
	EXPECT_FALSE(animals::InterestedInFood(flying));
	auto gone = hungry;
	gone.available = false;
	EXPECT_FALSE(animals::InterestedInFood(gone));
}

TEST(AnimalReactions, FoodHasTheTablePriorityOnlyWhenItHoldsFoodIsNearAndInterests)
{
	EXPECT_EQ(animals::FoodPriority(true, 10.0f, 20.0f, true, 120), 120u);
	EXPECT_EQ(animals::FoodPriority(true, 20.0f, 20.0f, true, 120), 120u);
	EXPECT_EQ(animals::FoodPriority(true, 20.5f, 20.0f, true, 120), 0u);
	EXPECT_EQ(animals::FoodPriority(false, 10.0f, 20.0f, true, 120), 0u);
	EXPECT_EQ(animals::FoodPriority(true, 10.0f, 20.0f, false, 120), 0u);
}

TEST(AnimalReactions, AnAnimalThatLooksTurnsByAStepOnTheShorterSide)
{
	// Nearer than the step, it faces the thing
	EXPECT_EQ(animals::LookTowards(100, 150, animals::k_LookTurnStep), 150);
	EXPECT_EQ(animals::LookTowards(150, 100, animals::k_LookTurnStep), 100);
	// Ahead by less than half a turn: forwards
	EXPECT_EQ(animals::LookTowards(100, 600, animals::k_LookTurnStep), 100 + 0x80);
	// Ahead by more than half a turn: backwards, round through 0
	EXPECT_EQ(animals::LookTowards(100, 1900, animals::k_LookTurnStep), (100 - 0x80) & 0x7FF);
	// Behind by less than half a turn: backwards
	EXPECT_EQ(animals::LookTowards(600, 100, animals::k_LookTurnStep), 600 - 0x80);
	// Behind by more than half a turn: forwards, round through 0
	EXPECT_EQ(animals::LookTowards(1900, 100, animals::k_LookTurnStep), (1900 + 0x80) & 0x7FF);
}

TEST(AnimalReactions, TheReactionStatesAreTheTablesFleeLookFollowInspectFoodWoodBallWatchAndHunterRows)
{
	for (uint8_t state = 0; state < 60; ++state)
	{
		const bool reaction = (state >= 6 && state <= 9) || (state >= 19 && state <= 22) || state == 25 || state == 26 ||
		                      state == 30 || state == 49;
		EXPECT_EQ(animals::IsReactionState(state), reaction) << static_cast<int>(state);
	}
}

TEST(AnimalReactions, AnAnimalThatDiesOrFallsInAReactionStateStopsTheReaction)
{
	// A cow that flees a fire and burns to death lies dead. The end of its reaction does not make it wander again
	constexpr uint8_t k_Dying = 14;
	constexpr uint8_t k_Downed = 17;
	for (uint8_t state = 0; state < 60; ++state)
	{
		EXPECT_EQ(animals::StateChangeEndsReaction(state, k_Dying), animals::IsReactionState(state)) << int(state);
		EXPECT_EQ(animals::StateChangeEndsReaction(state, k_Downed), animals::IsReactionState(state)) << int(state);
	}
}

TEST(AnimalReactions, AnAnimalThatGoesFromOneReactionStateToAnotherKeepsTheReaction)
{
	constexpr uint8_t k_Fleeing = 6;
	constexpr uint8_t k_WatchingWhatItFled = 30;
	constexpr uint8_t k_FleeingAHunter = 49;
	EXPECT_FALSE(animals::StateChangeEndsReaction(k_Fleeing, k_WatchingWhatItFled));
	EXPECT_FALSE(animals::StateChangeEndsReaction(k_WatchingWhatItFled, k_Fleeing));
	EXPECT_FALSE(animals::StateChangeEndsReaction(k_FleeingAHunter, k_Fleeing));
}

TEST(AnimalReactions, AnAnimalThatIsNotInAReactionStateKeepsItsReaction)
{
	constexpr uint8_t k_Wander = 32;
	constexpr uint8_t k_DecideWhatToDo = 43;
	constexpr uint8_t k_Dying = 14;
	EXPECT_FALSE(animals::StateChangeEndsReaction(k_Wander, k_Dying));
	EXPECT_FALSE(animals::StateChangeEndsReaction(k_DecideWhatToDo, k_Dying));
}

TEST(AnimalReactions, OnlyWanderSleepLairAndSeekFoodHaveAStartOfTheirOwn)
{
	for (size_t state = 0; state < 60; ++state)
	{
		const bool ownStart = state == 32 || state == 35 || state == 51 || state == 52;
		EXPECT_EQ(animals::StateHasOwnStart(state), ownStart) << state;
	}
}

TEST(AnimalSlope, TheLiftIsTheLandLessTheAnimalKeptBetweenTheLimits)
{
	EXPECT_FLOAT_EQ(animals::SlopeLift(10.1f, 10.0f), 10.1f - 10.0f);
	EXPECT_FLOAT_EQ(animals::SlopeLift(11.0f, 10.0f), 0.3f);
	EXPECT_FLOAT_EQ(animals::SlopeLift(9.0f, 10.0f), -0.3f);
	EXPECT_FLOAT_EQ(animals::SlopeLift(10.3f, 10.0f), 0.3f);
}

TEST(AnimalSlope, TheLandIsReadAtTheMetresTimes65536ThenATenth)
{
	EXPECT_EQ(animals::SlopeSampleUnit(1.0f), 6553);
	EXPECT_EQ(animals::SlopeSampleUnit(100.0f), 655360);
	EXPECT_EQ(animals::SlopeSampleUnit(-1.0f), -6553);
}

TEST(AnimalSlope, TheLeanLiftsTheXAndZAxesAlongTheUpAxis)
{
	const auto lean = animals::SlopeLean(0.1f, -0.2f);
	EXPECT_FLOAT_EQ(lean[0].y, 0.1f);
	EXPECT_FLOAT_EQ(lean[2].y, -0.2f);
	EXPECT_FLOAT_EQ(lean[1].y, 1.0f);
	EXPECT_FLOAT_EQ(lean[0].x, 1.0f);
	EXPECT_FLOAT_EQ(lean[2].z, 1.0f);
}

TEST(AnimalWanderStart, AGrazerOrAHunterStartsItsLegOnTheLand)
{
	EXPECT_EQ(animals::WanderStartOf(true, false), animals::WanderStart::OnTheLand);
	EXPECT_EQ(animals::WanderStartOf(false, true), animals::WanderStart::OnTheLand);
}

TEST(AnimalWanderStart, AnyOtherKindStartsItsLegAsALeader)
{
	EXPECT_EQ(animals::WanderStartOf(false, false), animals::WanderStart::AsLeader);
}
