/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Animals/FishFarmRules.h"
#include "Animals/FishShoal.h"

using namespace openblack;

namespace
{
constexpr fish_farm::Type k_Type {.foodValue = 100.0f, .foodType = 1, .turnsPerFish = 4};

/// Draws the middle of every range, and remembers the ranges asked for
struct MiddleRandom
{
	std::vector<std::pair<float, float>>* asked;
	float operator()(float a, float b) const
	{
		asked->emplace_back(a, b);
		return (a + b) * 0.5f;
	}
};
} // namespace

TEST(FishFarm, AFarmOfAKindOfFoodHoldsItsTablesFood)
{
	EXPECT_FLOAT_EQ(fish_farm::Full(k_Type), 100.0f);
	EXPECT_FLOAT_EQ(fish_farm::Full({.foodValue = 100.0f, .foodType = 4, .turnsPerFish = 4}), 0.0f);
}

TEST(FishFarm, AFishComesBackOnEverySoManyTurnsUpToFull)
{
	EXPECT_FLOAT_EQ(fish_farm::Grow(10.0f, 8, k_Type), 11.0f);
	EXPECT_FLOAT_EQ(fish_farm::Grow(10.0f, 9, k_Type), 10.0f);
	EXPECT_FLOAT_EQ(fish_farm::Grow(100.0f, 8, k_Type), 100.0f);
	EXPECT_FLOAT_EQ(fish_farm::Grow(-3.0f, 9, k_Type), 0.0f);
}

TEST(FishFarm, TakingFishTakesNoMoreThanItHas)
{
	float fish = 10.5f;
	EXPECT_EQ(fish_farm::Take(fish, 4), 4u);
	EXPECT_FLOAT_EQ(fish, 6.5f);
	EXPECT_EQ(fish_farm::Take(fish, 9), 6u);
	EXPECT_FLOAT_EQ(fish, 0.0f);
}

TEST(FishFarm, TheHandsFirstHandfulGoesByAFullFarmNotTheFishInIt)
{
	EXPECT_EQ(fish_farm::FirstHandful(25, k_Type), 25u);
	EXPECT_EQ(fish_farm::FirstHandful(250, k_Type), 100u);
}

TEST(FishFarm, AScoopWantsItsRampWithinTheHandfulsRoom)
{
	EXPECT_EQ(fish_farm::ScoopWanted(40, k_Type, 0, 0), 40u);
	EXPECT_EQ(fish_farm::ScoopWanted(400, k_Type, 0, 0), 100u);
	EXPECT_EQ(fish_farm::ScoopWanted(40, k_Type, 90, 100), 10u);
	EXPECT_EQ(fish_farm::ScoopWanted(40, k_Type, 120, 100), 0u);
}

TEST(FishFarm, ATownWantsAFarmFishedOnlyWhileItHasNoFisherman)
{
	EXPECT_EQ(fish_farm::DesireToBeFished(0, 4), 1u);
	EXPECT_EQ(fish_farm::DesireToBeFished(1, 4), 0u);
	EXPECT_EQ(fish_farm::DesireToBeFished(4, 4), 0u);
	EXPECT_EQ(fish_farm::DesireToBeFished(9, 4), 0u);
}

TEST(FishFarm, ATownSendsAFishermanToTheNearestFarmWithoutOne)
{
	const std::vector<fish_farm::Candidate> farms {
	    {.distance = 50.0f, .fishermen = 1},
	    {.distance = 200.0f, .fishermen = 0},
	    {.distance = 100.0f, .fishermen = 0},
	    {.distance = 100.0f, .fishermen = 0},
	};
	EXPECT_EQ(fish_farm::BestFarm(farms, 4), std::optional<size_t>(2));
	// None that has its fisherman, however near
	const std::vector<fish_farm::Candidate> none {{.distance = 5.0f, .fishermen = 1}, {.distance = 50.0f, .fishermen = 2}};
	EXPECT_FALSE(fish_farm::BestFarm(none, 4).has_value());
}

TEST(FishFarm, AFishermansSpotIsTheFarmMovedByMetres)
{
	// 2.5 m along x and -1 m along z from the middle of cell (10, 20)
	const glm::ivec2 farm {(10 << 16) | 0x8000, (20 << 16) | 0x8000};
	const auto spot = fish_farm::FishingSpot(farm, {2.5f, -1.0f});
	EXPECT_EQ(spot.x, farm.x + 16384);
	// Truncated towards 0 after the move: 6553.6 units back is 6554 whole ones
	EXPECT_EQ(spot.y, farm.y - 6554);
}

TEST(FishFarm, AFishermanCatchesAQuarterOfWhatHeCarriesLessOutOfSeason)
{
	// Spring: a quarter of 40
	EXPECT_EQ(fish_farm::Catch(40, 0, 0, 1.0f), 10);
	// Winter: 0.6 of it, truncated
	EXPECT_EQ(fish_farm::Catch(40, 0, 3, 1.0f), 6);
	// No more than his room, then by his tribe's skill
	EXPECT_EQ(fish_farm::Catch(40, 36, 0, 1.0f), 4);
	EXPECT_EQ(fish_farm::Catch(40, 0, 0, 1.5f), 15);
}

TEST(FishFarm, AFishermanTakesHisFoodToTheStoreOnceHisRoomIsLessThanHisCatch)
{
	EXPECT_FALSE(fish_farm::TakesCatchToStore(40, 20, 10));
	EXPECT_TRUE(fish_farm::TakesCatchToStore(40, 31, 10));
	EXPECT_TRUE(fish_farm::TakesCatchToStore(40, 40, 0));
	EXPECT_FALSE(fish_farm::TakesCatchToStore(40, 30, 10));
}

TEST(FishShoal, TheShoalLiesWhereTheSeaIsOpenOnTwoRingsRunning)
{
	// Open sea everywhere east of x = 5: the first direction, east, is open on the rings at 6 and 8 m
	const auto centre = fish_shoal::FindCentre({0.0f, 0.0f}, [](glm::vec2 point) { return point.x > 5.0f; });
	ASSERT_TRUE(centre.has_value());
	EXPECT_NEAR(centre->x, 8.0f, 1e-4f);
	EXPECT_NEAR(centre->y, 0.0f, 1e-4f);
	// No open sea: no shoal
	EXPECT_FALSE(fish_shoal::FindCentre({0.0f, 0.0f}, [](glm::vec2) { return false; }).has_value());
}

TEST(FishShoal, AsManyFishShowAsTheFarmIsFull)
{
	EXPECT_EQ(fish_shoal::ShownCount(1.0f), 15u);
	EXPECT_EQ(fish_shoal::ShownCount(0.5f), 7u);
	EXPECT_EQ(fish_shoal::ShownCount(1.0f / 16.0f), 0u);
}

TEST(FishShoal, AShoalFadesOutPastTwoHundredMetresAndIsGoneAtThreeHundred)
{
	EXPECT_EQ(fish_shoal::AlphaAt(100.0f * 100.0f), 255);
	EXPECT_EQ(fish_shoal::AlphaAt(48400.0f), 40);
	// Past where it has faded right out the game's opacity wraps round, as its byte does
	EXPECT_EQ(fish_shoal::AlphaAt(62500.0f), 194);
	EXPECT_FALSE(fish_shoal::AlphaAt(301.0f * 301.0f).has_value());
}

TEST(FishShoal, AFishIsMadeAboutItsCentre)
{
	std::vector<std::pair<float, float>> asked;
	const auto fish = fish_shoal::MakeFish({100.0f, -2.0f, 50.0f}, MiddleRandom {&asked});
	EXPECT_FLOAT_EQ(fish.size, 1.0f);
	EXPECT_EQ(fish.position, glm::vec3(100.0f, -2.5f, 50.0f));
	EXPECT_FLOAT_EQ(fish.speed, 1.0f);
	EXPECT_NEAR(fish.turnRate, 0.6283185f, 1e-6f);
	EXPECT_FLOAT_EQ(fish.panic, 0.0f);
	ASSERT_EQ(asked.size(), 10u);
	EXPECT_EQ(asked[4], std::make_pair(-5.0f, 5.0f));
	EXPECT_EQ(asked[5], std::make_pair(-1.0f, 0.0f));
}

TEST(FishShoal, AFishSwimsTheWayItFacesAndTurnsTowardsThePoint)
{
	fish_shoal::Fish fish {.position = {0.0f, -1.0f, 0.0f}, .heading = 0.0f, .speed = 1.0f, .turnRate = 1.0f};
	// The point lies to its left (towards +z): it turns that way
	fish_shoal::StepFish(fish, {10.0f, 0.0f, 10.0f}, 0.05f);
	EXPECT_NEAR(fish.position.x, 0.05f, 1e-6f);
	EXPECT_FLOAT_EQ(fish.position.y, -1.0f);
	EXPECT_NEAR(fish.heading, 0.05f, 1e-6f);
	// Its picture runs 25 frames a second for its speed, through frames 8 to 23
	EXPECT_EQ(fish.frame, 9u);
	// A long frame counts as a tenth of a second
	fish_shoal::StepFish(fish, {10.0f, 0.0f, 10.0f}, 5.0f);
	EXPECT_NEAR(fish.heading, 0.15f, 1e-5f);
}

TEST(FishShoal, APanickingFishDartsFourTimesAsFastSlowingOverItsLastSecond)
{
	fish_shoal::Fish fish {.heading = 0.0f, .speed = 1.0f, .turnRate = 0.0f, .panic = 2.0f};
	fish_shoal::StepFish(fish, {}, 0.1f);
	EXPECT_NEAR(fish.position.x, 0.4f, 1e-6f);
	fish.panic = 0.6f;
	fish.position = {};
	fish_shoal::StepFish(fish, {}, 0.1f);
	EXPECT_NEAR(fish.position.x, 0.1f * (1.0f + 3.0f * 0.5f), 1e-6f);
}

TEST(FishShoal, AScareSendsTheNearFishAwayAndTheShoalElsewhere)
{
	std::vector<std::pair<float, float>> asked;
	const MiddleRandom random {&asked};
	fish_shoal::Shoal shoal {.centre = {0.0f, 0.0f, 0.0f}, .target = {}, .retargetSeconds = 10.0f, .fullness = 0.14f};
	shoal.fish.at(0).position = {3.0f, 0.0f, 0.0f};
	shoal.fish.at(0).speed = 1.0f;
	shoal.fish.at(1).position = {30.0f, 0.0f, 0.0f};
	shoal.fish.at(2).position = {1.0f, 0.0f, 0.0f};
	fish_shoal::Step(shoal, 0.0f, glm::vec3(0.0f), random);
	// The near fish darts straight away; the far one and those not showing are left be
	EXPECT_FLOAT_EQ(shoal.fish.at(0).panic, 2.0f);
	EXPECT_NEAR(shoal.fish.at(0).heading, 0.0f, 1e-6f);
	EXPECT_FLOAT_EQ(shoal.fish.at(1).panic, 0.0f);
	EXPECT_FLOAT_EQ(shoal.fish.at(2).panic, 0.0f);
	EXPECT_FLOAT_EQ(shoal.retargetSeconds, 2.0f);
	// The shoal heads for a point two metres from its centre, at an angle drawn round the circle
	ASSERT_EQ(asked.size(), 1u);
	EXPECT_NEAR(glm::length(shoal.target), 2.0f, 1e-5f);
}

TEST(FishShoal, TheShoalsPointMovesOnWhenItsTimeIsUp)
{
	std::vector<std::pair<float, float>> asked;
	fish_shoal::Shoal shoal {
	    .centre = {10.0f, 0.0f, 10.0f}, .target = {10.0f, 0.0f, 16.0f}, .retargetSeconds = 0.05f, .fullness = 0.0f};
	fish_shoal::Step(shoal, 0.1f, std::nullopt, MiddleRandom {&asked});
	ASSERT_EQ(asked.size(), 2u);
	EXPECT_EQ(asked[0], std::make_pair(-7.0f, 7.0f));
	EXPECT_EQ(shoal.target, glm::vec3(10.0f, 0.0f, 10.0f));
	EXPECT_FLOAT_EQ(shoal.retargetSeconds, 3.0f);
}

TEST(FishShoal, TheHandFindsAShownFishWithinTwoMetres)
{
	fish_shoal::Shoal shoal {.fullness = 0.1f};
	shoal.fish.at(0).position = {10.0f, -1.0f, 10.0f};
	shoal.fish.at(1).position = {20.0f, -1.0f, 20.0f};
	EXPECT_TRUE(fish_shoal::HasShownFishNear(shoal, {11.0f, 11.0f}));
	EXPECT_FALSE(fish_shoal::HasShownFishNear(shoal, {12.0f, 10.0f}));
	// A fish that doesn't show isn't found
	EXPECT_FALSE(fish_shoal::HasShownFishNear(shoal, {20.0f, 20.0f}));
}

TEST(FishShoal, AFishLiesFlatAlongItsWayAndShowsItsFrame)
{
	const fish_shoal::Fish fish {.position = {0.0f, -1.0f, 0.0f}, .heading = std::numbers::pi_v<float> * 0.5f, .size = 1.0f};
	const auto corners = fish_shoal::Corners(fish);
	// Its picture's first column runs back from where it swims to
	EXPECT_NEAR(corners[0].x, 1.0f, 1e-6f);
	EXPECT_NEAR(corners[0].z, -1.0f, 1e-6f);
	EXPECT_NEAR(corners[1].z, 1.0f, 1e-6f);
	EXPECT_FLOAT_EQ(corners[2].y, -1.0f);
	const auto atlas = fish_shoal::AtlasCorners(10);
	EXPECT_EQ(atlas[0], glm::vec2(0.25f, 0.125f));
	EXPECT_EQ(atlas[2], glm::vec2(0.375f, 0.25f));
}
