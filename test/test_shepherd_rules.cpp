/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/ShepherdRules.h"

using namespace openblack;
using namespace openblack::ecs;
namespace rules = openblack::ecs::shepherd_rules;
using map_coords::MapCoords;

namespace
{
MapCoords At(float x, float z)
{
	return map_coords::FromMetres(glm::vec2(x, z));
}

float FlatGround(const MapCoords& /*at*/)
{
	return 0.0f;
}
} // namespace

TEST(ShepherdRules, ChoosesTheNearestFlockNoOneHerds)
{
	const std::array flocks {rules::FlockCandidate {.herded = true, .distance = 10.0f},
	                         rules::FlockCandidate {.herded = false, .distance = 120.0f},
	                         rules::FlockCandidate {.herded = false, .distance = 60.0f}};
	const auto choice = rules::ChooseBestFlock(flocks);
	ASSERT_TRUE(choice.has_value());
	EXPECT_EQ(choice->index, 2u);
	EXPECT_FLOAT_EQ(choice->score, gutils::GetDistanceModifier(60.0f, rules::k_FlockReach));
}

TEST(ShepherdRules, TheFirstOfEqualFlocksIsChosen)
{
	const std::array flocks {rules::FlockCandidate {.distance = 40.0f}, rules::FlockCandidate {.distance = 40.0f}};
	const auto choice = rules::ChooseBestFlock(flocks);
	ASSERT_TRUE(choice.has_value());
	EXPECT_EQ(choice->index, 0u);
}

TEST(ShepherdRules, AFarFlockCountsLessAndAHerdedOneNotAtAll)
{
	const std::array far {rules::FlockCandidate {.distance = rules::k_FlockReach}};
	const std::array near {rules::FlockCandidate {.distance = 10.0f}};
	EXPECT_LT(rules::ChooseBestFlock(far).value_or(rules::FlockChoice {}).score,
	          rules::ChooseBestFlock(near).value_or(rules::FlockChoice {}).score);
	const std::array herded {rules::FlockCandidate {.herded = true, .distance = 5.0f}};
	EXPECT_FALSE(rules::ChooseBestFlock(herded).has_value());
}

TEST(ShepherdRules, ALoneShepherdLetsItsFlockGo)
{
	EXPECT_EQ(rules::DecideWithFlock(1, 1.0f, 0.01f), rules::FlockDecision::Release);
	EXPECT_EQ(rules::DecideWithFlock(0, 1.0f, 0.01f), rules::FlockDecision::Release);
}

TEST(ShepherdRules, ATownWantingFoodHasAnAnimalTaken)
{
	// Two animals or more besides the shepherd: as soon as the town wants food enough
	EXPECT_EQ(rules::DecideWithFlock(3, 0.01f, 0.01f), rules::FlockDecision::Slaughter);
	// The last animal only when the town wants food badly
	EXPECT_EQ(rules::DecideWithFlock(2, 0.5f, 0.01f), rules::FlockDecision::MoveToWater);
	EXPECT_EQ(rules::DecideWithFlock(2, 0.51f, 0.01f), rules::FlockDecision::Slaughter);
	// Below its trigger the flock goes to drink
	EXPECT_EQ(rules::DecideWithFlock(5, 0.009f, 0.01f), rules::FlockDecision::MoveToWater);
}

TEST(ShepherdRules, TheWayToTheStoreGoesInShorterLegsNearTheDoor)
{
	EXPECT_EQ(rules::StoreLegFor(100.5f), rules::StoreLeg::Quarter);
	EXPECT_EQ(rules::StoreLegFor(100.0f), rules::StoreLeg::Step);
	EXPECT_EQ(rules::StoreLegFor(30.5f), rules::StoreLeg::Step);
	EXPECT_EQ(rules::StoreLegFor(30.0f), rules::StoreLeg::Door);
}

TEST(ShepherdRules, StoreLegGoals)
{
	const auto from = At(1000.0f, 1000.0f);
	const auto quarter = rules::StoreLegGoal(from, At(1200.0f, 1000.0f));
	EXPECT_NEAR(map_coords::ToMetres(quarter.x), 1050.0f, 0.05f);
	EXPECT_NEAR(map_coords::ToMetres(quarter.z), 1000.0f, 0.01f);
	const auto step = rules::StoreLegGoal(from, At(1000.0f, 1050.0f));
	EXPECT_NEAR(map_coords::ToMetres(step.x), 1000.0f, 0.05f);
	EXPECT_NEAR(map_coords::ToMetres(step.z), 1020.0f, 0.01f);
	const auto door = At(1010.0f, 1010.0f);
	EXPECT_EQ(rules::StoreLegGoal(from, door), door);
}

TEST(ShepherdRules, TheNearestAnimalIsNeitherTheShepherdNorAVillager)
{
	const auto from = At(500.0f, 500.0f);
	const std::array members {rules::MemberView {.position = from, .self = true},
	                          rules::MemberView {.position = At(501.0f, 500.0f), .villager = true},
	                          rules::MemberView {.position = At(503.0f, 500.0f)},
	                          rules::MemberView {.position = At(500.0f, 503.0f)},
	                          rules::MemberView {.position = At(502.0f, 500.0f)}};
	EXPECT_EQ(rules::NearestAnimal(members, from, 6.0f), std::optional<std::size_t>(4));
	// Strictly nearer than the reach
	EXPECT_EQ(rules::NearestAnimal(members, from, 1.5f), std::nullopt);
	// The first of equals
	const std::array equals {rules::MemberView {.position = At(503.0f, 500.0f)},
	                         rules::MemberView {.position = At(500.0f, 503.0f)}};
	EXPECT_EQ(rules::NearestAnimal(equals, from, 6.0f), std::optional<std::size_t>(0));
}

TEST(ShepherdRules, AnAnimalGivesItsFoodWhenItIsFood)
{
	EXPECT_EQ(rules::SlaughterFood(800.0f, FoodType::Meat), 800);
	EXPECT_EQ(rules::SlaughterFood(1200.5f, FoodType::Vegetable), 1200);
	EXPECT_EQ(rules::SlaughterFood(290.0f, FoodType::MeatVegetable), 290);
	EXPECT_EQ(rules::SlaughterFood(100.0f, FoodType::Graze), 0);
	EXPECT_EQ(rules::SlaughterFood(100.0f, FoodType::None), 0);
}

TEST(ShepherdRules, TheJobWorksInAnySeasonWithAnActivity)
{
	EXPECT_FALSE(rules::JobActive(JobActivity::None));
	EXPECT_TRUE(rules::JobActive(JobActivity::Nurture));
	EXPECT_TRUE(rules::JobActive(JobActivity::Harvest));
}

TEST(ShepherdRules, TheNearestRiverPointWithinReach)
{
	const std::vector points {At(1100.0f, 1000.0f), At(1030.0f, 1000.0f), At(1000.0f, 1040.0f)};
	const rules::River river {.points = points, .groundHeight = FlatGround};
	const auto found = rules::NearestRiverPoint(At(1000.0f, 1000.0f), river, 200.0f);
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ(found->x, points[1].x);
	EXPECT_EQ(found->z, points[1].z);
	EXPECT_FALSE(rules::NearestRiverPoint(At(1000.0f, 1000.0f), river, 30.0f).has_value());
}

TEST(ShepherdRules, TheCoastIsFoundInASpiralWithinReach)
{
	// One coastal cell 40 m east
	const auto isCoast = [](const MapCoords& at) { return map_coords::CellX(at) == 104 && map_coords::CellZ(at) == 100; };
	const auto from = At(1005.0f, 1005.0f);
	const auto coast = rules::NearestCoast(from, isCoast, 100.0f);
	ASSERT_TRUE(coast.has_value());
	EXPECT_EQ(map_coords::CellX(*coast), 104);
	EXPECT_EQ(map_coords::CellZ(*coast), 100);
	// Its place within the cell is kept
	EXPECT_EQ(coast->x & 0xFFFF, from.x & 0xFFFF);
	EXPECT_FALSE(rules::NearestCoast(from, isCoast, 30.0f).has_value());
}

TEST(ShepherdRules, ACoastNearerThanTheRiverIsTaken)
{
	const std::vector points {At(1065.0f, 1005.0f)};
	const rules::River river {.points = points, .groundHeight = FlatGround};
	const auto from = At(1005.0f, 1005.0f);
	const auto nearCoast = [](const MapCoords& at) { return map_coords::CellX(at) == 102 && map_coords::CellZ(at) == 100; };
	const auto water = rules::NearestDrinkingWater(from, river, nearCoast, 200.0f);
	ASSERT_TRUE(water.has_value());
	EXPECT_EQ(map_coords::CellX(*water), 102);
	// A coast further than the river is not
	const auto farCoast = [](const MapCoords& at) { return map_coords::CellX(at) == 110 && map_coords::CellZ(at) == 100; };
	const auto river2 = rules::NearestDrinkingWater(from, river, farCoast, 200.0f);
	ASSERT_TRUE(river2.has_value());
	EXPECT_EQ(river2->x, points[0].x);
	// With no river in reach, the coast within reach
	const auto none = [](const MapCoords& /*at*/) { return false; };
	EXPECT_FALSE(rules::NearestDrinkingWater(from, river, none, 50.0f).has_value());
}
