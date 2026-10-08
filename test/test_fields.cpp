/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Fields and fish farms, building side:
// fields::RemoveFood (the unsigned cost, the "> 150 carried" case, the unripe cost in float and the
// depletion), the growth step of a field's process, GetDesireToBeFarmed, the farmer / fisherman lists
// (newest first), the deletion listeners and their reset guard, RandomFarmPoint, the fish farm's score and RemoveFood,
// and the newest-first town lists (fields, fish farms, towns by creation stamp). No game data: a hand-made info.dat,
// entities without meshes.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GameRandomTesting.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Fields.h"
#include "ECS/FishFarms.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownQueries.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace fields = openblack::ecs::fields;
namespace fish_farms = openblack::ecs::fish_farms;
namespace map_coords = openblack::map_coords;

namespace
{
constexpr uint32_t k_TownId = 3;

class FieldsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		// the 6 field rows of info.dat are the same
		auto info = std::make_unique<InfoConstants>();
		for (auto& row : info->fieldType)
		{
			row.ageGrowth = 80.0f;
			row.ageRecolt = 1200.0f;
			row.timesToSow = 30.0f;
			row.foodValueTakenWithHand = 25.0f;
			row.totalFoodInField = 350.0f;
			row.maxFarmerInFarm = 10;
			row.effectSunWhenGrowing = 0.5f;
			row.effectSunWhenRipening = 1.5f;
			row.effectRainWhenGrowing = 1.5f;
			row.effectRainWhenRipening = 1.5f;
			row.ratioBeforeRipe = 0.2f;
			row.effectOfWaterSpell = 2.0f;
		}
		// the field's abode record (abode_queries::IsFunctional: thresholdForStopBeingFunctional < life 1)
		auto& abode = info->abode.at(0);
		abode.abodeNumber = AbodeNumber::Field;
		abode.thresholdForStopBeingFunctional = 0.5f;
		info->fishFarm.maxNoFishermanPerFishFarm = 4;
		info->fishFarm.numGameTurnsAfterWhichFoodIsIncreased = 16;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		auto& registry = Reg();
		_town = registry.Create();
		auto& town = registry.Assign<Town>(_town);
		town.id = k_TownId;
		registry.Context().towns[k_TownId] = _town;
	}

	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
	static const GFieldTypeInfo& Info() { return Locator::infoConstants::value().fieldType.at(0); }

	/// A fully sown field of the town at (x, 0, z), with a creation index
	static entt::entity MakeField(float growth, float food, int town = static_cast<int>(k_TownId), float x = 100.0f,
	                              float z = 200.0f)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		ecs::object_index::Assign(e);
		registry.Assign<Transform>(e, glm::vec3(x, 0.0f, z), glm::mat3(1.0f), glm::vec3(1.0f));
		auto& field = registry.Assign<Field>(e, town);
		field.crops = 30;
		field.growth = growth;
		field.food = food;
		return e;
	}

	static entt::entity MakeFishFarm(entt::entity town, float food = FishFarm::k_FoodValue)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		ecs::object_index::Assign(e);
		registry.Assign<Transform>(e, glm::vec3(50.0f, 0.0f, 60.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		auto& farm = registry.Assign<FishFarm>(e);
		farm.town = town;
		farm.food = food;
		return e;
	}

	static Field& F(entt::entity e) { return Reg().Get<Field>(e); }
	Town& TownData() { return Reg().Get<Town>(_town); }

	entt::entity _town {entt::null};
};

// ---- fields::RemoveFood ------------------------------------------------------------------------------------------

TEST_F(FieldsTest, RemoveFoodNothingWhenEmptyOrNotSown)
{
	const auto empty = MakeField(1200.0f, 0.0f);
	EXPECT_EQ(fields::RemoveFood(empty, 25.0f), 0); // food == 0
	const auto unsown = MakeField(1200.0f, 350.0f);
	F(unsown).crops = 29; // (float)crops < timesToSow
	EXPECT_EQ(fields::RemoveFood(unsown, 25.0f), 0);
	EXPECT_EQ(F(unsown).food, 350.0f);
}

TEST_F(FieldsTest, RemoveFoodRipeTakesK)
{
	const auto field = MakeField(1200.0f, 350.0f);
	EXPECT_EQ(fields::RemoveFood(field, 25.9f), 25); // k = ftol(25.9) = 25, cost = k
	EXPECT_FLOAT_EQ(F(field).food, 325.0f);
	EXPECT_EQ(TownData().buildPulse, 0u); // not run out: no pulse
}

TEST_F(FieldsTest, RemoveFoodUnripeCostsRatioPlusK)
{
	const auto field = MakeField(100.0f, 100.0f);
	// cost = ftol(25 x 0.2 + 25) = 30; the villager still gets k = 25
	EXPECT_EQ(fields::RemoveFood(field, 25.0f), 25);
	EXPECT_FLOAT_EQ(F(field).food, 70.0f);
	// fractional amount: cost = ftol(2.5 x 0.2 + 2) = 2 (openblack's old int(1.2 x 2.5) was 3)
	EXPECT_EQ(fields::RemoveFood(field, 2.5f), 2);
	EXPECT_FLOAT_EQ(F(field).food, 68.0f);
}

TEST_F(FieldsTest, RemoveFoodUnripeCostRoundsToFloat)
{
	// the 24-bit FPU: 29.99999 x 0.2 = 5.999998f, + 29 = 35.0f (rounded), ftol 35; in double it
	// would be 34.999998 -> 34
	const auto field = MakeField(100.0f, 100.0f);
	EXPECT_EQ(fields::RemoveFood(field, 29.99999f), 29);
	EXPECT_FLOAT_EQ(F(field).food, 65.0f);
}

TEST_F(FieldsTest, RemoveFoodNegativeAmountComparesUnsigned)
{
	// FarmerDigsUpCrop with more than 150 carried: amount 150 - 160 = -10, k = cost = -10; (u32)-10 is above
	// any food, so the ripe field runs out and gives ALL its food
	const auto field = MakeField(1200.0f, 350.0f);
	TownData().buildPulsePrevious = 7;
	EXPECT_EQ(fields::RemoveFood(field, -10.0f), 350);
	EXPECT_EQ(F(field).food, 0.0f);
	EXPECT_EQ(F(field).crops, 0);
	EXPECT_EQ(F(field).growth, 0.0f);
	// the town's pulse
	EXPECT_EQ(TownData().buildPulse, 1u);
	EXPECT_EQ(TownData().buildPulsePrevious, 0u);
}

TEST_F(FieldsTest, RemoveFoodRipeRunsOutOnEqualCost)
{
	// (u32)cost < food is strict: 25 of 25 runs out (ftol(food) = 25) and clears the field
	const auto field = MakeField(1200.0f, 25.0f);
	EXPECT_EQ(fields::RemoveFood(field, 25.0f), 25);
	EXPECT_EQ(F(field).crops, 0);
	EXPECT_EQ(F(field).growth, 0.0f);
}

TEST_F(FieldsTest, RemoveFoodUnripeRunsOutKeepsTheCrop)
{
	// unripe, cost 30 >= 20: food = 0, ftol(25 x 0.2) = 5; crops and growth stay
	const auto field = MakeField(100.0f, 20.0f);
	EXPECT_EQ(fields::RemoveFood(field, 25.0f), 5);
	EXPECT_EQ(F(field).food, 0.0f);
	EXPECT_EQ(F(field).crops, 30);
	EXPECT_EQ(F(field).growth, 100.0f);
	EXPECT_EQ(TownData().buildPulse, 1u);
}

TEST_F(FieldsTest, RemoveFoodWithoutTownNoPulse)
{
	const auto field = MakeField(1200.0f, 10.0f, -1); // CREATE_FIELD: town -1
	EXPECT_EQ(fields::RemoveFood(field, 25.0f), 10);
	EXPECT_EQ(TownData().buildPulse, 0u);
	EXPECT_TRUE(fields::TownOf(field) == entt::null);
}

// ---- a field's process: the step -------------------------------------------------------------------------------

TEST_F(FieldsTest, GrowthStepMultipliers)
{
	// a = 2 (alignment x 0.5 + 1): 2 on neutral land, 3 on good land (1), 1 on evil land (-1)
	EXPECT_FLOAT_EQ(fields::GrowthStep(0.0f, Info(), 0.0f, false), 1.0f);  // growing, sun 0.5
	EXPECT_FLOAT_EQ(fields::GrowthStep(80.0f, Info(), 0.0f, false), 3.0f); // ripening from ageGrowth on, sun 1.5
	EXPECT_FLOAT_EQ(fields::GrowthStep(0.0f, Info(), 0.0f, true), 3.0f);   // growing in the rain, 1.5
	EXPECT_FLOAT_EQ(fields::GrowthStep(500.0f, Info(), 0.0f, true), 3.0f); // ripening in the rain, 1.5
	EXPECT_FLOAT_EQ(fields::GrowthStep(0.0f, Info(), 1.0f, false), 1.5f);  // good land
	EXPECT_FLOAT_EQ(fields::GrowthStep(0.0f, Info(), -1.0f, false), 0.5f); // evil land
	EXPECT_FLOAT_EQ(fields::GrowthStep(79.9f, Info(), 0.5f, true), 3.75f); // 2.5 x 1.5
}

// ---- the farmers --------------------------------------------------------------------------------------------------

TEST_F(FieldsTest, FarmersNewestFirstNoDuplicatesNoMaximum)
{
	const auto field = MakeField(0.0f, 0.0f);
	std::vector<entt::entity> villagers;
	for (int i = 0; i < 12; ++i)
	{
		villagers.push_back(Reg().Create());
		fields::AddFarmer(field, villagers.back());
	}
	fields::AddFarmer(field, villagers[3]);     // already in: nothing
	fields::AddFarmer(field, entt::null);       // null: nothing
	EXPECT_EQ(fields::FarmerCount(field), 12u); // no maximum (maxFarmerInFarm 10)
	EXPECT_EQ(F(field).farmers.front(), villagers.back());
	EXPECT_EQ(F(field).farmers.back(), villagers.front());
	fields::RemoveFarmer(field, villagers[5]);
	EXPECT_EQ(fields::FarmerCount(field), 11u);
	EXPECT_FALSE(fields::HasFarmer(field, villagers[5]));
	// not in the list: nothing to unlink (its TargetThing write is still to do)
	fields::RemoveFarmer(field, Reg().Create());
	EXPECT_EQ(fields::FarmerCount(field), 11u);
}

TEST_F(FieldsTest, ActivityAndPercentFull)
{
	const auto field = MakeField(0.0f, 0.0f);
	F(field).crops = 15;
	EXPECT_FLOAT_EQ(fields::GetPercentFull(field), 0.5f);
	EXPECT_EQ(fields::GetFieldActivity(field), 1); // to sow
	EXPECT_TRUE(fields::IsStillSowing(field));
	EXPECT_TRUE(fields::PlantCrop(field));
	EXPECT_EQ(F(field).crops, 16);
	F(field).crops = 30;
	EXPECT_FALSE(fields::PlantCrop(field));
	EXPECT_EQ(fields::GetFieldActivity(field), 0); // growing: nothing to do
	F(field).growth = 80.0f;
	EXPECT_EQ(fields::GetFieldActivity(field), 2); // from ageGrowth on: harvest
	EXPECT_EQ(fields::GetFoodValue(field), 0.0f);  // not ripe yet
	F(field).growth = 1200.0f;
	F(field).food = 350.0f;
	EXPECT_EQ(fields::GetFoodValue(field), 350.0f);
}

TEST_F(FieldsTest, RandomAndRipeFarmPoint)
{
	game_random::testing::ScopedState state;
	uint32_t draws = 0;
	game_random::testing::SetGameRand(nullptr, [&draws](float range) {
		++draws;
		return draws == 1 ? 0.25f * range : 0.75f * range; // r1 = 5 - 2.5, r2 = 5 - 7.5
	});
	const auto field = MakeField(100.0f, 0.0f);
	map_coords::MapCoords out {};
	EXPECT_FALSE(fields::RipeFarmPoint(field, out)); // unripe: no draw
	EXPECT_EQ(draws, 0u);
	F(field).growth = 1200.0f;
	ASSERT_TRUE(fields::RipeFarmPoint(field, out));
	EXPECT_EQ(draws, 2u);
	EXPECT_NEAR(map_coords::ToMetres(out.x), 102.5f, 0.001f);
	EXPECT_NEAR(map_coords::ToMetres(out.z), 197.5f, 0.001f);
	EXPECT_TRUE(fields::IsTouching(field, out));
	EXPECT_FALSE(fields::IsTouching(field, map_coords::FromMetres({105.0f, 200.0f}))); // fx + 5 is outside
	EXPECT_TRUE(fields::IsTouching(field, map_coords::FromMetres({95.0f, 200.0f})));   // fx - 5 is inside
}

TEST_F(FieldsTest, DesireToBeFarmed)
{
	// a built field (Abode defaults: built, 100 %) of the field record
	const auto field = MakeField(0.0f, 0.0f);
	Reg().Assign<Abode>(field, AbodeNumber::Field, k_TownId, 0u, 0u);
	F(field).crops = 15; // p = 0.5, activity 1: (1 - p) a^3
	std::vector<entt::entity> villagers;
	const auto addFarmers = [&](size_t n) {
		while (fields::FarmerCount(field) < n)
		{
			villagers.push_back(Reg().Create());
			fields::AddFarmer(field, villagers.back());
		}
	};
	EXPECT_FLOAT_EQ(fields::GetDesireToBeFarmed(field), 0.5f); // a = 1
	addFarmers(5);
	EXPECT_FLOAT_EQ(fields::GetDesireToBeFarmed(field), 0.0625f); // a = 0.5: 0.5 x 0.125
	addFarmers(10);
	EXPECT_EQ(fields::GetDesireToBeFarmed(field), 0.0f); // a = 0
	addFarmers(12);
	EXPECT_EQ(fields::GetDesireToBeFarmed(field), 0.0f); // the share is held at 1
	// activity 2 (harvest): a when ripe, else 0
	for (const auto v : villagers)
	{
		fields::RemoveFarmer(field, v);
	}
	F(field).crops = 30;
	F(field).growth = 100.0f;
	EXPECT_EQ(fields::GetDesireToBeFarmed(field), 0.0f);
	F(field).growth = 1200.0f;
	EXPECT_EQ(fields::GetDesireToBeFarmed(field), 1.0f);
	// not functional (under construction) -> 0
	Reg().Get<Abode>(field).percentBuilt = 0.5f;
	EXPECT_EQ(fields::GetDesireToBeFarmed(field), 0.0f);
}

TEST_F(FieldsTest, TownsNewestFirstByCreationStamp)
{
	// Town::id is the CREATE_TOWN argument (out of order on some maps, e.g. 5 0 4 3 2 1): the stamp decides
	TownData().creationStamp = 1;
	auto& registry = Reg();
	const auto second = registry.Create();
	registry.Assign<Town>(second, 0u).creationStamp = 2;
	const auto third = registry.Create();
	registry.Assign<Town>(third, 9u).creationStamp = 3;
	EXPECT_EQ(ecs::town_queries::TownsNewestFirst(), (std::vector<entt::entity> {third, second, _town}));
}

TEST_F(FieldsTest, DeletionListenersDisconnectBeforeReset)
{
	// connected by the first AddFarmer / AddFisherman; Game::LoadMap disconnects them before Registry::Reset
	const auto field = MakeField(0.0f, 0.0f);
	const auto farm = MakeFishFarm(_town);
	EXPECT_TRUE(Reg().OnDestroy<Field>().empty());
	fields::AddFarmer(field, Reg().Create());
	fish_farms::AddFisherman(farm, Reg().Create());
	EXPECT_FALSE(Reg().OnDestroy<Field>().empty());
	EXPECT_FALSE(Reg().OnDestroy<FishFarm>().empty());
	fields::DisconnectDeletionListeners();
	EXPECT_TRUE(Reg().OnDestroy<Field>().empty());
	EXPECT_TRUE(Reg().OnDestroy<FishFarm>().empty());
	const auto seeds = game_random::Current();
	Reg().Reset();
	EXPECT_EQ(game_random::Current().synced, seeds.synced);
}

TEST_F(FieldsTest, TownFieldsNewestFirst)
{
	const auto a = MakeField(0.0f, 0.0f);
	const auto other = MakeField(0.0f, 0.0f, 9);
	const auto b = MakeField(0.0f, 0.0f);
	const auto c = MakeField(0.0f, 0.0f);
	EXPECT_EQ(fields::TownFields(_town), (std::vector<entt::entity> {c, b, a}));
	EXPECT_TRUE(fields::IsField(other));
	EXPECT_EQ(fields::TownOf(a), _town);
}

TEST_F(FieldsTest, DeleteDependantsWalksEveryFarmerOnce)
{
	// plain entities (no LivingAction): villager::SetTopState does nothing and nobody unlinks them; the loop still
	// ends, the next being taken from the list before each call
	const auto field = MakeField(0.0f, 0.0f);
	const auto v1 = Reg().Create();
	const auto v2 = Reg().Create();
	fields::AddFarmer(field, v1);
	fields::AddFarmer(field, v2);
	fields::DeleteDependants(field);
	EXPECT_EQ(F(field).farmers, (std::vector<entt::entity> {v2, v1}));
}

TEST_F(FieldsTest, DestroyRunsDeleteDependants)
{
	// the on_destroy<Field> / on_destroy<FishFarm> listeners (ecs::ToBeDeleted ends in Registry::Destroy); the fish
	// farm's loop is ended by its guard (nobody unlinks the plain entity)
	const auto field = MakeField(0.0f, 0.0f);
	const auto farm = MakeFishFarm(_town);
	const auto farmer = Reg().Create();
	const auto fisherman = Reg().Create();
	fields::AddFarmer(field, farmer);
	fish_farms::AddFisherman(farm, fisherman);
	Reg().Destroy(field);
	Reg().Destroy(farm);
	EXPECT_FALSE(Reg().Valid(field));
	EXPECT_FALSE(Reg().Valid(farm));
	EXPECT_TRUE(Reg().Valid(farmer));
	EXPECT_TRUE(Reg().Valid(fisherman));
}

// ---- fish farms ----------------------------------------------------------------------------------------------------

TEST_F(FieldsTest, FishermenNewestFirstAndScore)
{
	const auto farm = MakeFishFarm(_town);
	const auto v1 = Reg().Create();
	const auto v2 = Reg().Create();
	EXPECT_EQ(fish_farms::Score(farm), 1); // no fisherman
	fish_farms::AddFisherman(farm, v1);
	fish_farms::AddFisherman(farm, v2);
	EXPECT_EQ(Reg().Get<FishFarm>(farm).fishermen, (std::vector<entt::entity> {v2, v1}));
	EXPECT_EQ(fish_farms::Score(farm), 0); // ftol(1 - 2 / 4) = 0
	fish_farms::RemoveFisherman(farm, v1);
	EXPECT_EQ(fish_farms::FishermanCount(farm), 1u);
}

TEST_F(FieldsTest, FishFarmRemoveFoodAndTownList)
{
	const auto farm = MakeFishFarm(_town, 30.0f);
	EXPECT_EQ(fish_farms::RemoveFood(farm, 30), 30); // n <= food
	EXPECT_EQ(Reg().Get<FishFarm>(farm).food, 0.0f);
	Reg().Get<FishFarm>(farm).food = 12.5f;
	EXPECT_EQ(fish_farms::RemoveFood(farm, 20), 12); // ftol(food)
	EXPECT_EQ(fish_farms::RemoveResource(farm, ResourceType::Wood, 5), 0);
	const auto shoalOnly = MakeFishFarm(entt::null); // the fish puzzle's: no town
	const auto newer = MakeFishFarm(_town);
	EXPECT_EQ(fish_farms::TownFishFarms(_town), (std::vector<entt::entity> {newer, farm}));
	EXPECT_TRUE(fish_farms::TownFishFarms(entt::null).empty());
	EXPECT_TRUE(fish_farms::IsFishFarm(shoalOnly));
}

TEST_F(FieldsTest, FishFarmDeleteDependantsStopsWithoutUnlink)
{
	const auto farm = MakeFishFarm(_town);
	fish_farms::AddFisherman(farm, Reg().Create());
	// no unlinking villager side: the guard ends the loop after one call
	fish_farms::DeleteDependants(farm);
	EXPECT_EQ(fish_farms::FishermanCount(farm), 1u);
}
// A field's process starts with the abode's process, whose turn counter counts every call, so a town's field,
// processed by the global list and by the town's pass on the same turn, counts twice
TEST_F(FieldsTest, ProcessFieldRunsTheAbodePartEachCall)
{
	const auto field = MakeField(0.0f, 0.0f);
	Reg().Assign<Abode>(field, AbodeNumber::Field, k_TownId, 0u, 0u);
	F(field).turnOffset = 0;
	const auto before = Reg().Get<Abode>(field).field0xB9;
	ecs::ProcessField(field, 3); // (3 + 0) % 10 != 0: no growth this turn
	ecs::ProcessField(field, 3);
	EXPECT_EQ(Reg().Get<Abode>(field).field0xB9, before + 2);
	EXPECT_EQ(F(field).growth, 0.0f);
}
} // namespace

// ---- the pure cores (characterisation of the formulas) -------------------------------------------------------------

TEST(FishFarmScore, OneOnlyWithoutFishermen)
{
	EXPECT_EQ(fish_farms::ScoreFor(0, 4), 1);
	EXPECT_EQ(fish_farms::ScoreFor(1, 4), 0); // truncated 0.75
	EXPECT_EQ(fish_farms::ScoreFor(2, 4), 0);
	EXPECT_EQ(fish_farms::ScoreFor(4, 4), 0);
	EXPECT_EQ(fish_farms::ScoreFor(5, 4), 0); // the share stops at 1
}

TEST(FishFarmSpot, MovedInMetresAndTruncated)
{
	const map_coords::MapCoords farm {.x = 65536, .z = 131072, .altitude = 3.5f};
	const auto spot = fish_farms::FishingSpotAt(farm, 1.0f, -2.5f);
	EXPECT_EQ(spot.x, 72089);  // 11 m: 72089.6 truncated
	EXPECT_EQ(spot.z, 114688); // 17.5 m
	EXPECT_EQ(spot.altitude, 3.5f);
	EXPECT_EQ(fish_farms::FishingSpotAt({.x = 7, .z = 100}, 0.0f, 0.0f), (map_coords::MapCoords {.x = 7, .z = 100}));
}

namespace
{
int g_WaterReads = 0;
float CountedWater()
{
	++g_WaterReads;
	return 2.0f;
}
} // namespace

TEST(FieldWaterSpell, SowsGrowsOrLeavesTheField)
{
	g_WaterReads = 0;
	Field unsown;
	unsown.crops = 0;
	fields::ApplyWaterSpell(unsown, CountedWater);
	EXPECT_EQ(unsown.crops, 31); // sown at once
	EXPECT_EQ(unsown.growth, 0.0f);
	EXPECT_EQ(g_WaterReads, 0); // the info is not read

	Field growing;
	growing.crops = 31;
	growing.growth = 100.0f;
	fields::ApplyWaterSpell(growing, CountedWater);
	EXPECT_EQ(growing.growth, 102.0f);
	EXPECT_FLOAT_EQ(growing.food, 2.0f * 350.0f / 1200.0f);
	EXPECT_EQ(g_WaterReads, 1);

	Field ripe;
	ripe.crops = 31;
	ripe.growth = 1200.5f;
	fields::ApplyWaterSpell(ripe, CountedWater);
	EXPECT_EQ(ripe.growth, 1200.5f); // past ripe: nothing
	EXPECT_EQ(g_WaterReads, 1);
}
