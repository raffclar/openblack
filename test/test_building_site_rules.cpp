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
#include <vector>

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "ECS/BuildingSiteRules.h"
#include "ECS/VillagerCarry.h"

using namespace openblack;
using namespace openblack::building_site;

namespace
{
/// A random source that answers what it was told, as the game's draws would
FloatRandom FixedFloat(float value)
{
	return [value](float) { return value; };
}
IntRandom FixedInt(uint32_t value)
{
	return [value](uint32_t) { return value; };
}
} // namespace

TEST(BuildingSiteRules, BuildersNeededIsTheMostLessThoseWorking)
{
	EXPECT_EQ(BuildersNeeded({.maxBuilders = 6, .workers = 2, .built = false, .repaired = false, .repairDesire = 0.0f}), 4);
	// More may work at it than it takes
	EXPECT_EQ(BuildersNeeded({.maxBuilders = 6, .workers = 9, .built = false, .repaired = false, .repairDesire = 0.0f}), -3);
	// A built one wants none once whole, or when nobody wants it mended
	EXPECT_EQ(BuildersNeeded({.maxBuilders = 6, .workers = 0, .built = true, .repaired = true, .repairDesire = 1.0f}), 0);
	EXPECT_EQ(BuildersNeeded({.maxBuilders = 6, .workers = 0, .built = true, .repaired = false, .repairDesire = 0.0f}), 0);
	EXPECT_EQ(BuildersNeeded({.maxBuilders = 6, .workers = 1, .built = true, .repaired = false, .repairDesire = 0.5f}), 5);
}

TEST(BuildingSiteRules, DesireForVillagersIsHeldBetweenNoneAndAll)
{
	EXPECT_FLOAT_EQ(DesireForVillagers(3, 6, 0.0f), 0.5f);
	EXPECT_FLOAT_EQ(DesireForVillagers(3, 6, 5.0f), 1.0f);
	EXPECT_FLOAT_EQ(DesireForVillagers(-12, 6, 0.0f), 0.0f);
}

TEST(BuildingSiteRules, WoodValueAndWoodStillNeeded)
{
	EXPECT_FLOAT_EQ(WoodValue(1.0f, 20000, 1.0f), 20000.0f);
	EXPECT_FLOAT_EQ(WoodValue(1.5f, 2000, 2.0f), 1500.0f);
	// A quarter built, with 1000 at the site
	EXPECT_FLOAT_EQ(WoodNeededToBuild(0.25f, 8000.0f, 1000), 5000.0f);
}

TEST(BuildingSiteRules, NearTheBuildingAVillagerFetchesOnlyWhenTheSiteHasNoWood)
{
	ShouldFetchInputs in {.woodAtSite = 0,
	                      .woodNeeded = 1000.0f,
	                      .distanceToBuilding = 10.0f,
	                      .distanceToDropOff = 500.0f,
	                      .woodHeld = 148,
	                      .maxWoodCarried = 250,
	                      .woodPerBuilderWanted = 50};
	EXPECT_TRUE(ShouldFetchWood(in));
	in.woodAtSite = 1;
	EXPECT_FALSE(ShouldFetchWood(in));
}

TEST(BuildingSiteRules, FarFromTheBuildingAVillagerWeighsTheWays)
{
	const std::array<int16_t, 2> workers {0, 0};
	ShouldFetchInputs in {.woodAtSite = 0,
	                      .workersWood = workers,
	                      .woodNeeded = 1000.0f,
	                      .distanceToBuilding = 100.0f,
	                      .distanceToDropOff = 10.0f,
	                      .woodHeld = 0,
	                      .maxWoodCarried = 250,
	                      .woodPerBuilderWanted = 50};
	// Empty-handed, with nothing at the site and the store nearer: it fetches
	EXPECT_TRUE(ShouldFetchWood(in));
	// Carrying a full load it goes straight to build
	in.woodHeld = 250;
	EXPECT_FALSE(ShouldFetchWood(in));
	// Nothing more wanted than lies there and is carried
	in.woodHeld = 0;
	in.woodAtSite = 1000;
	EXPECT_FALSE(ShouldFetchWood(in));
}

TEST(BuildingSiteRules, AStrokeUsesMoreWoodOnEvilLand)
{
	EXPECT_EQ(WoodPerStroke(50.0f, 1.0f), 50);
	EXPECT_EQ(WoodPerStroke(50.0f, 0.0f), 50);
	EXPECT_EQ(WoodPerStroke(50.0f, -0.5f), 55);
	EXPECT_EQ(WoodPerStroke(50.0f, -1.0f), 60);
	EXPECT_EQ(WoodPerStroke(40.0f, -1.0f), 48);
}

TEST(BuildingSiteRules, PlaceAtWrapsTheAngleRound)
{
	EXPECT_EQ(PlaceAt(0.0f), 0u);
	EXPECT_EQ(PlaceAt(3.1415927f), 64u);
	EXPECT_EQ(PlaceAt(-3.1415927f), 64u);
	EXPECT_EQ(PlaceAt(6.2831855f + 0.1f), PlaceAt(0.1f));
	// Beyond three turns forward it is half a turn's place, and backward the first
	EXPECT_EQ(PlaceAt(30.0f), 64u);
	EXPECT_EQ(PlaceAt(-30.0f), 0u);
}

TEST(BuildingSiteRules, NextPlaceStepsRoundOneWayOrTheOther)
{
	// A building 10 m across has about 2 places in 2 m of its outline
	const float radius = 10.0f;
	const float perOutline = 2.0f / (radius * 6.2831855f * 0.0078125f);
	const auto step = static_cast<uint32_t>(perOutline);
	EXPECT_EQ(NextPlace(10, radius, FixedFloat(0.0f), FixedInt(1)), 10 + step);
	EXPECT_EQ(NextPlace(10, radius, FixedFloat(0.0f), FixedInt(0)), 10 - step);
	// Round past either end
	EXPECT_EQ(NextPlace(127, radius, FixedFloat(0.0f), FixedInt(1)), step - 1);
	EXPECT_EQ(NextPlace(0, radius, FixedFloat(0.0f), FixedInt(0)), 128 - step);
}

TEST(BuildingSiteRules, FirstPlaceIsWithinAnEighthTurnOfTheBuilder)
{
	EXPECT_FLOAT_EQ(FirstPlaceAngle(1.0f, FixedFloat(0.0f)), 1.0f - 0.78539819f);
	EXPECT_FLOAT_EQ(FirstPlaceAngle(1.0f, FixedFloat(1.5707964f)), 1.0f + 0.78539819f);
}

TEST(BuildingSiteRules, OutlinePlacesFollowTheFootOfASquare)
{
	// A square 10 m a side about the origin, its walls rising from the ground; a roof triangle up high doesn't count
	std::vector<Triangle> triangles;
	const std::array<glm::vec3, 4> corners {glm::vec3(-5, 0, -5), glm::vec3(5, 0, -5), glm::vec3(5, 0, 5), glm::vec3(-5, 0, 5)};
	for (size_t i = 0; i < corners.size(); ++i)
	{
		const auto a = corners.at(i);
		const auto b = corners.at((i + 1) % corners.size());
		triangles.push_back({{a, b, b + glm::vec3(0, 10, 0)}});
	}
	triangles.push_back({{glm::vec3(-5, 10, -5), glm::vec3(5, 10, -5), glm::vec3(0, 12, 0)}});
	const auto places = OutlinePlaces(triangles, glm::mat4(1.0f), glm::vec3(0.0f), 20.0f);
	// Straight along +x the wall is 5 m out, and the place a metre beyond it
	EXPECT_NEAR(places.at(0).x, 6.0f, 1e-4f);
	EXPECT_NEAR(places.at(0).z, 0.0f, 1e-4f);
	// A sixteenth of a turn round the wall is crossed off the axis, and the place is put a metre farther along the way
	const float z = 5.0f * std::tan(8.0f * 0.049087387f);
	const float out = std::sqrt(25.0f + z * z);
	EXPECT_NEAR(places.at(8).x, 5.0f * (out + 1.0f) / out, 1e-3f);
	EXPECT_NEAR(places.at(8).z, z * (out + 1.0f) / out, 1e-3f);
}

TEST(BuildingSiteRules, OutlinePlacesWithNoFootStayAtTheBuilding)
{
	const std::vector<Triangle> triangles {{{glm::vec3(-5, 3, -5), glm::vec3(5, 3, -5), glm::vec3(0, 4, 0)}}};
	const auto model = glm::mat4(1.0f);
	const auto places = OutlinePlaces(triangles, model, glm::vec3(0.0f, 0.0f, 2.0f), 20.0f);
	// The building's place, put a metre out from the box's middle
	EXPECT_NEAR(places.at(5).z, -1.0f, 1e-5f);
	EXPECT_NEAR(places.at(5).x, 0.0f, 1e-5f);
}

TEST(BuildingSiteRules, TemplePilesAreASeventhOfATurnApart)
{
	EXPECT_FLOAT_EQ(TemplePileAngle(0.0f, 0), -1.1423974f);
	EXPECT_FLOAT_EQ(TemplePileAngle(0.5f, 2), 0.5f + 2.0f * 0.89759791f - 1.1423974f);
}

TEST(BuildingSiteRules, BestSiteWeighsDistanceByDesire)
{
	const std::array<SiteCandidate, 3> sites {
	    SiteCandidate {.wantsBuilders = true, .distanceToEdge = 100.0f, .desireForVillagers = 1.0f},
	    SiteCandidate {.wantsBuilders = true, .distanceToEdge = 50.0f, .desireForVillagers = 1.0f},
	    SiteCandidate {.wantsBuilders = false, .distanceToEdge = 1.0f, .desireForVillagers = 0.0f},
	};
	EXPECT_EQ(BestSite(sites, false), 1u);
	// A builder disciple takes even the one that wants nobody
	EXPECT_EQ(BestSite(sites, true), 2u);
	EXPECT_FALSE(BestSite(std::span(sites).last(1), false).has_value());
}

TEST(BuildingSiteRules, ForABuildingTheStoreCountsOnlyWhenItHasMoreThanTheRoom)
{
	WoodSourceInputs in {.forBuilding = true,
	                     .distanceToStore = 100.0f,
	                     .storeWood = 30000,
	                     .room = 102,
	                     .maxWoodCarried = 250,
	                     .distanceToForest = std::nullopt,
	                     .forestHasBigForest = false,
	                     .maxDistance = 250.0f};
	EXPECT_EQ(DecideWoodSource(in), WoodSource::Store);
	in.storeWood = 50;
	EXPECT_EQ(DecideWoodSource(in), WoodSource::None);
	// A forest beside it counts half, which beats a far store
	in.storeWood = 30000;
	in.distanceToStore = 200.0f;
	in.distanceToForest = 0.0f;
	EXPECT_EQ(DecideWoodSource(in), WoodSource::Forest);
	in.forestHasBigForest = true;
	EXPECT_EQ(DecideWoodSource(in), WoodSource::BigForest);
}

TEST(VillagerCarry, PickingUpAndPuttingDown)
{
	ecs::components::Villager villager {};
	EXPECT_EQ(ecs::villager_carry::PickUp(villager, ResourceType::Wood, 148, 6), 148);
	EXPECT_EQ(villager.woodHeld, 148);
	// Only four logs
	EXPECT_EQ(villager.woodGraphic, 2);
	ecs::villager_carry::PickUp(villager, ResourceType::Food, 148, 0);
	EXPECT_EQ(villager.foodHeld, 148);
	EXPECT_EQ(ecs::villager_carry::Room(villager, ResourceType::Wood, 150, 250), 102);
	EXPECT_EQ(ecs::villager_carry::HeldResource(villager), ResourceType::Wood);
	// Putting down none, or more than it has, puts down all
	EXPECT_EQ(ecs::villager_carry::PutDown(villager, ResourceType::Wood, 0), 148);
	EXPECT_EQ(villager.woodHeld, 0);
	EXPECT_EQ(ecs::villager_carry::PutDown(villager, ResourceType::Food, 500), 148);
	EXPECT_EQ(ecs::villager_carry::PutDown(villager, ResourceType::Food, 0), 0);
	EXPECT_EQ(ecs::villager_carry::HeldResource(villager), ResourceType::None);
}
