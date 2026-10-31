/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/GameEndConditions.h"

using namespace openblack;
using namespace openblack::game_end;
using openblack::ecs::components::GameStanding;
using openblack::ecs::components::WinCondition;

namespace
{
/// A standing with one condition of a target
GameStanding StandingWith(Condition condition, int32_t target)
{
	GameStanding standing;
	standing.conditions.at(static_cast<size_t>(condition)) = WinCondition {.current = 0, .target = target, .completed = false};
	return standing;
}

const WinCondition& ConditionOf(const GameStanding& standing, Condition condition)
{
	return *standing.conditions.at(static_cast<size_t>(condition));
}
} // namespace

TEST(GameEndConditions, TheTimeLimitIsSixHundredTurnsAMinute)
{
	EXPECT_EQ(TimeLimitTurn(k_DefaultTimeLimitMinutes), 18000u);
	EXPECT_EQ(TimeLimitTurn(1), 600u);
	EXPECT_EQ(TimeLimitTurn(1440), 864000u);
}

TEST(GameEndConditions, TheOperatorTimeLimitKeepsToTheLimitsOfTheHost)
{
	// A build for players accepts the limits of the host's controls only: 10 minutes to 24 hours
	EXPECT_EQ(TimeLimitRefusal(9, false), "the time limit must be 10 minutes or more");
	EXPECT_FALSE(TimeLimitRefusal(k_ShortestTimeLimitMinutes, false).has_value());
	EXPECT_FALSE(TimeLimitRefusal(k_DefaultTimeLimitMinutes, false).has_value());
	EXPECT_FALSE(TimeLimitRefusal(k_LongestTimeLimitMinutes, false).has_value());
	EXPECT_EQ(TimeLimitRefusal(1441, false), "the time limit must be 1440 minutes (24 hours) or less");
	EXPECT_TRUE(TimeLimitRefusal(0, false).has_value());
	// A build with the developer tools also accepts a short limit for tests, but never 0 and never more than 24 hours
	EXPECT_FALSE(TimeLimitRefusal(1, true).has_value());
	EXPECT_EQ(TimeLimitRefusal(0, true), "the time limit must be 1 minute or more");
	EXPECT_TRUE(TimeLimitRefusal(1441, true).has_value());
}

TEST(GameEndConditions, OnlyAConditionThatThePlayerHasCounts)
{
	auto standing = StandingWith(Condition::VillagersBorn, 3);
	AddToCondition(standing, Condition::VillagersKilled, 5);
	EXPECT_FALSE(standing.conditions.at(static_cast<size_t>(Condition::VillagersKilled)).has_value());
	AddToCondition(standing, Condition::VillagersBorn, 2);
	EXPECT_EQ(ConditionOf(standing, Condition::VillagersBorn).current, 2);
	EXPECT_FALSE(ConditionOf(standing, Condition::VillagersBorn).completed);
	AddToCondition(standing, Condition::VillagersBorn, 1);
	EXPECT_TRUE(ConditionOf(standing, Condition::VillagersBorn).completed);
	EXPECT_TRUE(HasCondition(standing, Condition::VillagersBorn));
	EXPECT_FALSE(HasCondition(standing, Condition::TreesGrown));
}

TEST(GameEndConditions, AMeasuredConditionStaysCompletedWhenItsValueFalls)
{
	auto standing = StandingWith(Condition::WoodInStores, 100);
	SetCondition(standing, Condition::WoodInStores, 120);
	EXPECT_TRUE(ConditionOf(standing, Condition::WoodInStores).completed);
	SetCondition(standing, Condition::WoodInStores, 10);
	EXPECT_EQ(ConditionOf(standing, Condition::WoodInStores).current, 10);
	EXPECT_TRUE(ConditionOf(standing, Condition::WoodInStores).completed);
	SetCondition(standing, Condition::FoodInStores, 500);
	EXPECT_FALSE(standing.conditions.at(static_cast<size_t>(Condition::FoodInStores)).has_value());
}

TEST(GameEndConditions, NaturalDeathsDoNotCountAsKilled)
{
	EXPECT_FALSE(CountsAsKilled(DeathReason::None));
	EXPECT_FALSE(CountsAsKilled(DeathReason::Starving));
	EXPECT_FALSE(CountsAsKilled(DeathReason::Animal));
	EXPECT_FALSE(CountsAsKilled(DeathReason::Chant));
	EXPECT_FALSE(CountsAsKilled(DeathReason::Exhaustion));
	EXPECT_FALSE(CountsAsKilled(DeathReason::OldAge));
	EXPECT_TRUE(CountsAsKilled(DeathReason::Spell));
	EXPECT_TRUE(CountsAsKilled(DeathReason::PlayerInteraction));
	EXPECT_TRUE(CountsAsKilled(DeathReason::PlayerInteractionDrown));
	EXPECT_TRUE(CountsAsKilled(DeathReason::Sacrifice));
}

TEST(GameEndConditions, TheFirstHarmToAWholeBuildingCountsAsSmashed)
{
	// Whole before, hurt but still working
	EXPECT_EQ(SmashedCondition({.lifeBefore = 1.0f, .lifeAfter = 0.8f, .stopsWorkingAt = 0.3f, .byPlayer = true}),
	          Condition::BuildingsSmashedByPlayer);
	// Hurt before: a second harm that leaves it working counts nothing
	EXPECT_FALSE(SmashedCondition({.lifeBefore = 0.8f, .lifeAfter = 0.6f, .stopsWorkingAt = 0.3f, .byPlayer = true}));
	// All its life left counts nothing
	EXPECT_FALSE(SmashedCondition({.lifeBefore = 1.0f, .lifeAfter = 1.0f, .stopsWorkingAt = 0.3f, .byPlayer = true}));
	// A building that already stopped working counts nothing more
	EXPECT_FALSE(SmashedCondition({.lifeBefore = 0.2f, .lifeAfter = 0.1f, .stopsWorkingAt = 0.3f, .byPlayer = true}));
	// A creature did it
	EXPECT_EQ(
	    SmashedCondition({.lifeBefore = 1.0f, .lifeAfter = 0.8f, .stopsWorkingAt = 0.3f, .byPlayer = true, .byCreature = true}),
	    Condition::BuildingsSmashedByCreature);
}

TEST(GameEndConditions, ABuildingThatStopsWorkingCountsOnlyWhenSettledAndHarmedByAPlayer)
{
	constexpr BuildingHarm k_Stops {.lifeBefore = 0.6f, .lifeAfter = 0.2f, .stopsWorkingAt = 0.3f, .byPlayer = true};
	auto settled = k_Stops;
	settled.age = k_SettledAge;
	EXPECT_EQ(SmashedCondition(settled), Condition::BuildingsSmashedByPlayer);
	// A new building does not count, even when it was whole before
	auto young = k_Stops;
	young.lifeBefore = 1.0f;
	young.age = k_SettledAge - 1;
	EXPECT_FALSE(SmashedCondition(young));
	// No player did it
	auto nobody = settled;
	nobody.byPlayer = false;
	EXPECT_FALSE(SmashedCondition(nobody));
	// Exactly at the life where it stops working, it stops
	auto atLimit = settled;
	atLimit.lifeAfter = 0.3f;
	atLimit.byCreature = true;
	EXPECT_EQ(SmashedCondition(atLimit), Condition::BuildingsSmashedByCreature);
}

TEST(GameEndConditions, ANewTownNeedsAStoragePitATownCentreAndAHouse)
{
	const std::vector<TownBuilding> two {{.type = AbodeType::StoragePit}, {.type = AbodeType::TownCentre}};
	EXPECT_TRUE(CompletesNewTown(two, AbodeType::LivingQuarters));
	// A windmill is not a house
	EXPECT_FALSE(CompletesNewTown(two, AbodeType::Windmill));
	EXPECT_FALSE(CompletesNewTown(two, AbodeType::Creche));
	// A building of the town that is not all built does not count
	const std::vector<TownBuilding> unbuilt {{.type = AbodeType::StoragePit, .built = false}, {.type = AbodeType::TownCentre}};
	EXPECT_FALSE(CompletesNewTown(unbuilt, AbodeType::LivingQuarters));
	// The building that starts to work counts, and the unbuilt storage pit of the town does not stop it
	const std::vector<TownBuilding> withHouse {
	    {.type = AbodeType::StoragePit, .built = false}, {.type = AbodeType::TownCentre}, {.type = AbodeType::LivingQuarters}};
	EXPECT_TRUE(CompletesNewTown(withHouse, AbodeType::StoragePit));
}

TEST(GameEndConditions, ATownThatHadAllThreeIsNotNewAgain)
{
	const std::vector<TownBuilding> complete {
	    {.type = AbodeType::StoragePit}, {.type = AbodeType::TownCentre}, {.type = AbodeType::LivingQuarters}};
	EXPECT_FALSE(CompletesNewTown(complete, AbodeType::LivingQuarters));
	EXPECT_FALSE(CompletesNewTown(complete, AbodeType::StoragePit));
	EXPECT_FALSE(CompletesNewTown({}, AbodeType::TownCentre));
}

TEST(GameEndConditions, TheStoresAddTheirPiles)
{
	EXPECT_EQ(AmountInStores({}), 0);
	constexpr std::array<uint32_t, 5> k_Wood {10, 0, 25, 3, 2};
	EXPECT_EQ(AmountInStores(k_Wood), 40);
}

TEST(GameEndConditions, ThePrayerPowerIsAWholeNumberOfTheSum)
{
	EXPECT_EQ(PrayerPower({}), 0);
	constexpr std::array k_Sites {10.75f, 20.5f};
	EXPECT_EQ(PrayerPower(k_Sites), 31);
	// A site with infinite prayer power
	constexpr std::array k_Infinite {1.0e6f, 0.5f};
	EXPECT_EQ(PrayerPower(k_Infinite), 1000000);
	// The sum rounds to a float at each step: a small value does not change a large sum
	constexpr std::array k_Rounded {16777216.0f, 1.0f};
	EXPECT_EQ(PrayerPower(k_Rounded), 16777216);
}

TEST(GameEndConditions, TheBeliefOfEachTownIsAWholeNumberOfThousandths)
{
	EXPECT_EQ(BeliefInWorld({}), 0);
	// 0.0015 thousandfold is 1.5, made 1 before the sum
	constexpr std::array k_Towns {0.0015f, 0.0015f, 2.0f};
	EXPECT_EQ(BeliefInWorld(k_Towns), 2002);
	// A belief that is not a whole number of thousandths in float
	constexpr std::array k_Third {std::bit_cast<float>(0x3EAAAAABu)};
	EXPECT_EQ(BeliefInWorld(k_Third), 333);
}

TEST(GameEndConditions, TheGodGetsEachPickedConditionFromZero)
{
	GameStanding standing;
	constexpr std::array k_Picks {ConditionPick {.condition = Condition::HousesBuilt, .value = 3},
	                              ConditionPick {.condition = Condition::TreesGrown, .value = 5}};
	GiveConditions(standing, k_Picks);
	ASSERT_TRUE(HasCondition(standing, Condition::HousesBuilt));
	EXPECT_EQ(standing.conditions.at(static_cast<size_t>(Condition::HousesBuilt))->target, 3);
	EXPECT_EQ(standing.conditions.at(static_cast<size_t>(Condition::HousesBuilt))->current, 0);
	EXPECT_FALSE(standing.conditions.at(static_cast<size_t>(Condition::HousesBuilt))->completed);
	EXPECT_EQ(standing.conditions.at(static_cast<size_t>(Condition::TreesGrown))->target, 5);
	// A condition that the host did not pick stays empty, so it counts nothing
	EXPECT_FALSE(HasCondition(standing, Condition::VillagersKilled));
	AddToCondition(standing, Condition::VillagersKilled, 1);
	EXPECT_FALSE(HasCondition(standing, Condition::VillagersKilled));
	// A picked condition counts up to its value
	AddToCondition(standing, Condition::HousesBuilt, 3);
	EXPECT_TRUE(standing.conditions.at(static_cast<size_t>(Condition::HousesBuilt))->completed);
}
