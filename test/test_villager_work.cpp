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
#include <optional>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/BuildingSiteRules.h"
#include "ECS/TownDesire.h"
#include "ECS/VillagerWorkRules.h"

using namespace openblack;
using namespace openblack::ecs;
namespace work = openblack::ecs::villager_work;

namespace
{
work::WoodInterestInputs Interested()
{
	return {.available = true,
	        .room = 250,
	        .maxCarried = 250,
	        .held = 0,
	        .hasTown = true,
	        .healthy = true,
	        .free = true,
	        .townWoodDesire = 0.0f,
	        .stateInterest = 1.0f,
	        .distanceModifier = 1.0f,
	        .townTrigger = 0.01f};
}
} // namespace

TEST(VillagerWork, AForesterStandsByTheTrunkItsOwnSizeAndALittleMoreAway)
{
	// The step of 0.5 + 0.9 metres is 9175 whole map units along x, added to the tree's map position as an integer
	const map_coords::MapCoords tree {.x = 0x400001, .z = 0x400000, .altitude = 1.5f};
	const map_coords::MapCoords villager {.x = 0x410000, .z = 0x400000};
	const auto at = work::TreeWorkingPosition(tree, villager, 0.5f);
	EXPECT_EQ(at.x, 0x400001 + 9175);
	EXPECT_EQ(at.z, 0x400000);
	EXPECT_EQ(at.altitude, 1.5f);
}

TEST(VillagerWork, TheTreeWorkingSpotKeepsEveryUnitFarOutOnTheMap)
{
	// Near the far edge of the map a metre value cannot hold each map unit. The step is still added exactly. A goal
	// found in metres and changed back to map units ended 2 units short here.
	const map_coords::MapCoords tree {.x = 0x1FB5355, .z = 0x1F00005};
	const map_coords::MapCoords villager {.x = 0x1FD0000, .z = 0x1F00005};
	const auto at = work::TreeWorkingPosition(tree, villager, 0.5f);
	EXPECT_EQ(at.x, 0x1FB5355 + 9175);
	EXPECT_EQ(at.z, 0x1F00005);
}

TEST(VillagerWork, AVillagerStandsByAThingBothTheirSizesAway)
{
	// 2 + 0.5 metres is 16384 whole map units, here towards a villager at lower x
	const map_coords::MapCoords thing {.x = 0x320007, .z = 0x320000};
	const map_coords::MapCoords villager {.x = 0x310000, .z = 0x320000};
	const auto edge = work::EdgeWorkingPosition(thing, villager, 2.0f, 0.5f);
	EXPECT_EQ(edge.x, 0x320007 - 16384);
	EXPECT_EQ(edge.z, 0x320000);
}

TEST(VillagerWork, TheNearestEdgeOfAThingGoesThroughMetresAsTheGameGivesIt)
{
	// The edge is 2 + 0.5 metres (16384 map units) towards a villager at lower x, at 8090858. That position in metres
	// and back is 8090857. The altitude is 0.
	const map_coords::MapCoords thing {.x = 8090858 + 16384, .z = 0x320000, .altitude = 2.0f};
	const map_coords::MapCoords villager {.x = 0x310000, .z = 0x320000};
	const auto edge = work::NearestEdgePosition(thing, villager, 2.0f, 0.5f);
	EXPECT_EQ(edge.x, 8090857);
	EXPECT_EQ(edge.z, 0x320000);
	EXPECT_FLOAT_EQ(edge.altitude, 0.0f);
}

TEST(VillagerWork, ABigForestGivesWoodHalfItsRadiusFromItsMiddle)
{
	// Half of a radius of 30 metres is 98304 whole map units
	const map_coords::MapCoords bigForest {.x = 0x100009, .z = 0x200000};
	const map_coords::MapCoords villager {.x = 0x140000, .z = 0x200000};
	const auto big = work::BigForestArrivePosition(bigForest, villager, 30.0f);
	EXPECT_EQ(big.x, 0x100009 + 98304);
	EXPECT_EQ(big.z, 0x200000);
}

TEST(VillagerWork, ThePointOfAPileNearestAVillagerKeepsEveryUnitOfThePile)
{
	// 2.5 metres is 16384 whole map units, here towards a villager at higher x. The pile is far out on the map, where
	// metres cannot hold each map unit.
	const map_coords::MapCoords pile {.x = 0x1FB5355, .z = 0x1F00005, .altitude = 0.25f};
	const map_coords::MapCoords villager {.x = 0x1FD0000, .z = 0x1F00005};
	const auto edge = work::EdgeWorkingPosition(pile, villager, 2.5f, 0.0f);
	EXPECT_EQ(edge.x, 0x1FB5355 + 16384);
	EXPECT_EQ(edge.z, 0x1F00005);
	EXPECT_EQ(edge.altitude, 0.25f);
}

TEST(VillagerWork, AMapPositionThroughMetresCanMoveAUnit)
{
	const auto far = work::ThroughMetres({.x = 0x1FB5355, .z = 0x1234567, .altitude = 3.0f});
	EXPECT_EQ(far.x, 0x1FB5356);
	EXPECT_EQ(far.z, 0x1234566);
	EXPECT_EQ(far.altitude, 0.0f);
	const auto near = work::ThroughMetres({.x = 0x400001, .z = 0x320000});
	EXPECT_EQ(near.x, 0x400001);
	EXPECT_EQ(near.z, 0x320000);
}

TEST(VillagerWork, TouchingIsTheEdgeNearerThanAStep)
{
	EXPECT_TRUE(work::Touching(0.6f, 0.5f, 0.2f));
	EXPECT_FALSE(work::Touching(0.8f, 0.5f, 0.2f));
	const auto at = map_coords::FromMetres({10.0f, 10.0f});
	EXPECT_FALSE(work::AreWeThere(at, at, 0.0f, 0.0f));
	EXPECT_TRUE(work::AreWeThere(at, at, 1.0f, 0.0f));
}

TEST(VillagerWork, MovingWoodIsWaitedForAHundredSeconds)
{
	EXPECT_EQ(work::TurnsToWaitForMovingWood(100), 1000u);
}

TEST(VillagerWork, OnlyAForesterAtWorkGoesForWoodAgainAtOnce)
{
	EXPECT_TRUE(work::ReactsToWoodAgainAtOnce(VillagerStates::ForesterChopsTree));
	EXPECT_TRUE(work::ReactsToWoodAgainAtOnce(VillagerStates::ForesterFinishedForestering));
	EXPECT_FALSE(work::ReactsToWoodAgainAtOnce(VillagerStates::ForesterChopsTreeForBuilding));
	EXPECT_FALSE(work::ReactsToWoodAgainAtOnce(VillagerStates::ArrivesAtBigForest));
}

TEST(VillagerWork, TheLandsInteractDesireAddsToTheInterestInWood)
{
	auto in = Interested();
	// The town does not want wood, so the wood does not catch the eye
	EXPECT_FALSE(work::IsInterestedInWood(in));
	// The value that the land's script sets goes past the town's trigger by itself
	in.interactDesire = 0.02f;
	EXPECT_TRUE(work::IsInterestedInWood(in));
}

TEST(VillagerWork, AFallenTrunkCatchesTheEyeWhileItsLoadIsUnderItsRoom)
{
	auto in = Interested();
	in.fallenTree = true;
	EXPECT_TRUE(work::IsInterestedInWood(in));
	// Carrying 150 leaves room for 100: the trunk no longer counts by itself
	in.held = 150;
	in.room = 100;
	EXPECT_FALSE(work::IsInterestedInWood(in));
	in = Interested();
	in.fallenTree = true;
	in.room = 0;
	EXPECT_FALSE(work::IsInterestedInWood(in));
	in = Interested();
	in.fallenTree = true;
	in.healthy = false;
	EXPECT_FALSE(work::IsInterestedInWood(in));
}

TEST(VillagerWork, WoodDisciplesGoForAnyWoodUnlessTheirSiteHasEnough)
{
	auto in = Interested();
	in.disciplePicksUpWood = true;
	EXPECT_TRUE(work::IsInterestedInWood(in));
	in.siteHasEnough = true;
	EXPECT_FALSE(work::IsInterestedInWood(in));
}

TEST(VillagerWork, OtherwiseItsTownsWantOfWoodMovesIt)
{
	auto in = Interested();
	EXPECT_FALSE(work::IsInterestedInWood(in));
	in.townWoodDesire = 0.5f;
	EXPECT_TRUE(work::IsInterestedInWood(in));
	in.room = 2;
	EXPECT_FALSE(work::IsInterestedInWood(in));
}

TEST(VillagerWork, ASitesPileDrawsAFreeVillagerWhereAPileTheHandMadeNeedsItsTownToWantWood)
{
	// Its town wants no wood: a pile the hand let go of is passed by
	auto in = Interested();
	EXPECT_FALSE(work::IsInterestedInWood(in));
	// A site's pile, even one left over when its temple is finished, is gone for while the villager is free
	in.siteWood = true;
	EXPECT_TRUE(work::IsInterestedInWood(in));
	in.free = false;
	EXPECT_FALSE(work::IsInterestedInWood(in));
	// Nor by a builder whose own site already has the wood it needs
	in.free = true;
	in.siteHasEnough = true;
	EXPECT_FALSE(work::IsInterestedInWood(in));
}

TEST(VillagerWork, TakingWoodFromAFallenTrunkShrinksItOrUsesItUp)
{
	const auto part = work::TakeFromFallenTree(350, 250, 350, 1.0f);
	EXPECT_EQ(part.taken, 250u);
	ASSERT_TRUE(part.scale.has_value());
	EXPECT_NEAR(*part.scale, 100.0f / 350.0f, 1e-6f);
	const auto all = work::TakeFromFallenTree(100, 250, 350, 1.0f);
	EXPECT_EQ(all.taken, 100u);
	EXPECT_FALSE(all.scale.has_value());
}

TEST(VillagerWork, ATownSendsItsPeopleToAnOrdinaryForestBeforeItsLoneTrees)
{
	const std::array<work::ForestCandidate, 3> forests {
	    {{.distance = 10.0f, .scenic = true}, {.distance = 120.0f, .scenic = false}, {.distance = 60.0f, .scenic = false}}};
	EXPECT_EQ(work::NearestTownForest(forests, 250.0f), std::optional<size_t>(2));
	const std::array<work::ForestCandidate, 2> scenic {{{.distance = 10.0f, .scenic = true}, {.distance = 300.0f}}};
	EXPECT_EQ(work::NearestTownForest(scenic, 250.0f), std::optional<size_t>(0));
	const std::array<work::ForestCandidate, 1> far {{{.distance = 260.0f}}};
	EXPECT_FALSE(work::NearestTownForest(far, 250.0f).has_value());
}

TEST(VillagerWork, TheCentreTreeIsTheNearerOfTheGrownAndGrowing)
{
	EXPECT_EQ(work::CentreTree(1.0f, 2.0f), std::optional<size_t>(0));
	EXPECT_EQ(work::CentreTree(2.0f, 2.0f), std::optional<size_t>(0));
	EXPECT_EQ(work::CentreTree(3.0f, 2.0f), std::optional<size_t>(1));
	EXPECT_EQ(work::CentreTree(std::nanf(""), 2.0f), std::optional<size_t>(0));
	EXPECT_EQ(work::CentreTree(std::nullopt, 2.0f), std::optional<size_t>(1));
	EXPECT_FALSE(work::CentreTree(std::nullopt, std::nullopt).has_value());
}

TEST(VillagerWork, WithWoodADiscipleDecidesAndAnOrdinaryVillagerBuildsOrStores)
{
	EXPECT_EQ(work::AfterGettingWood(0, false, true), work::GotWood::Decide);
	EXPECT_EQ(work::AfterGettingWood(250, true, true), work::GotWood::Decide);
	EXPECT_EQ(work::AfterGettingWood(250, false, true), work::GotWood::BuildingSite);
	EXPECT_EQ(work::AfterGettingWood(250, false, false), work::GotWood::LookForSiteOrStore);
}

TEST(VillagerWork, AFishermanFishesFromAnywhereInTheFarmsCellButTheFarmsOwnPlace)
{
	const map_coords::MapCoords farm {0x10008000, 0x20008000, 0.0f};
	EXPECT_EQ(work::ArriveAtFishing(farm, farm), work::FishingArrival::MoveToSpot);
	EXPECT_EQ(work::ArriveAtFishing({0x10000100, 0x2000F000, 0.0f}, farm), work::FishingArrival::StartFishing);
	EXPECT_EQ(work::ArriveAtFishing({0x10018000, 0x20008000, 0.0f}, farm), work::FishingArrival::WalkAgain);
	EXPECT_TRUE(work::IsAtValidFishingPos({0x1000FFFF, 0x20000000, 0.0f}, farm));
}

TEST(VillagerWork, TheBestFoodJobWinsUnlessCarryingHomeScoresMore)
{
	const std::array<work::FoodJobCandidate, 3> jobs {{{.job = work::FoodJob::FishFarm, .score = 0.5f, .found = true},
	                                                   {.job = work::FoodJob::Field, .score = 0.5f, .found = true},
	                                                   {.job = work::FoodJob::Flock}}};
	// Empty handed, the first of equals: the fish farm
	auto choice = work::ChooseFoodJob(jobs, 150, 150, 1.0f);
	EXPECT_FALSE(choice.dropOff);
	EXPECT_EQ(choice.job, std::optional(work::FoodJob::FishFarm));
	// Full, the store wins
	choice = work::ChooseFoodJob(jobs, 0, 150, 0.9f);
	EXPECT_TRUE(choice.dropOff);
	const std::array<work::FoodJobCandidate, 3> none {
	    {{.job = work::FoodJob::FishFarm}, {.job = work::FoodJob::Field}, {.job = work::FoodJob::Flock}}};
	choice = work::ChooseFoodJob(none, 150, 150, 1.0f);
	EXPECT_FALSE(choice.dropOff);
	EXPECT_FALSE(choice.job.has_value());
}

TEST(VillagerWork, AFieldWantsSowersThenHarvestersOnceRipe)
{
	work::FieldDesireInputs field {.timesSown = 15,
	                               .timesToSow = 30.0f,
	                               .age = 0.0f,
	                               .ageGrowth = 80.0f,
	                               .ageRecolt = 1200.0f,
	                               .farmers = 0,
	                               .maxFarmers = 10};
	EXPECT_FLOAT_EQ(work::DesireToBeFarmed(field), 0.5f);
	field.farmers = 5;
	EXPECT_FLOAT_EQ(work::DesireToBeFarmed(field), 0.5f * 0.125f);
	field.farmers = 0;
	field.timesSown = 30;
	field.age = 600.0f;
	EXPECT_FLOAT_EQ(work::DesireToBeFarmed(field), 0.0f);
	field.age = 1200.0f;
	EXPECT_FLOAT_EQ(work::DesireToBeFarmed(field), 1.0f);
}

TEST(VillagerWork, AnEmptyHandedForesterGoesToTheForestAndAFullOneHome)
{
	building_site::WoodSourceInputs in {.forBuilding = false,
	                                    .distanceToStore = 50.0f,
	                                    .storeWood = 10000,
	                                    .room = 250,
	                                    .maxWoodCarried = 250,
	                                    .distanceToForest = 100.0f,
	                                    .forestHasBigForest = false,
	                                    .maxDistance = 250.0f};
	EXPECT_EQ(building_site::DecideWoodSource(in), building_site::WoodSource::Forest);
	in.forestHasBigForest = true;
	EXPECT_EQ(building_site::DecideWoodSource(in), building_site::WoodSource::BigForest);
	in.room = 0;
	EXPECT_EQ(building_site::DecideWoodSource(in), building_site::WoodSource::Store);
}

TEST(VillagerWork, ATownCountsWhatItsPeopleDoByTheirStates)
{
	const auto in = town_desire::DoingNowChange({.desire = 1, .weight = 1.0f}, 1.0f);
	ASSERT_TRUE(in.has_value());
	EXPECT_EQ(in->desire, 1u);
	EXPECT_FLOAT_EQ(in->amount, 1.0f);
	EXPECT_FLOAT_EQ(in->count, 1.0f);
	const auto out = town_desire::DoingNowChange({.desire = 0, .weight = 0.5f}, -1.0f);
	ASSERT_TRUE(out.has_value());
	EXPECT_FLOAT_EQ(out->amount, -0.5f);
	EXPECT_FLOAT_EQ(out->count, -1.0f);
	EXPECT_FALSE(town_desire::DoingNowChange({.desire = -1, .weight = 1.0f}, 1.0f).has_value());
}

TEST(VillagerWork, AFarmerWorksAtSpotsUpToFiveMetresEitherWayOfTheFieldsMiddle)
{
	// The first draw moves it along x, the second along z
	const auto spot = work::FieldWorkSpot({100.0f, 200.0f}, 0.0f, 10.0f);
	EXPECT_EQ(spot.x, 688128);
	EXPECT_EQ(spot.y, 1277952);
	const auto middle = work::FieldWorkSpot({100.0f, 200.0f}, 5.0f, 5.0f);
	EXPECT_EQ(middle.x, 655360);
	EXPECT_EQ(middle.y, 1310720);
}

TEST(VillagerWork, AFarmerSowsOrDigsOnceAtItsSpotAndOtherwiseDecidesAfresh)
{
	using enum work::FarmerArrival;
	EXPECT_EQ(work::ArriveAtField(true, false, false, false), WalkToSpot);
	EXPECT_EQ(work::ArriveAtField(true, false, false, true), Sow);
	EXPECT_EQ(work::ArriveAtField(false, true, true, false), WalkToSpot);
	EXPECT_EQ(work::ArriveAtField(false, true, true, true), Dig);
	// Grown but not yet ripe, or still growing: nothing to do
	EXPECT_EQ(work::ArriveAtField(false, true, false, true), Decide);
	EXPECT_EQ(work::ArriveAtField(false, false, false, true), Decide);
}

TEST(VillagerWork, TheTestOfAGrownFieldForARipeCropFindsASpotThatItDoesNotKeep)
{
	// A grown and ripe field: the test finds a spot, also when the farmer is not at its spot
	EXPECT_TRUE(work::RipeTestFindsASpot(false, true, true));
	// A field to sow, a grown field that is not ripe and a field that grows: no spot
	EXPECT_FALSE(work::RipeTestFindsASpot(true, false, false));
	EXPECT_FALSE(work::RipeTestFindsASpot(false, true, false));
	EXPECT_FALSE(work::RipeTestFindsASpot(false, false, false));
}

TEST(VillagerWork, AFarmerTakesItsHarvestToTheStoreOnceItHasLessRoomThanALoadOfCrop)
{
	// A full load of 150 leaves no room
	EXPECT_TRUE(work::TakesHarvestToStore(150, 0, 100.0f));
	// After the last 50 of a field it still has room for 100: it goes on, to sow the field again
	EXPECT_FALSE(work::TakesHarvestToStore(50, 100, 100.0f));
	EXPECT_TRUE(work::TakesHarvestToStore(51, 99, 100.0f));
	// Getting nothing it never goes
	EXPECT_FALSE(work::TakesHarvestToStore(0, 0, 100.0f));
}
