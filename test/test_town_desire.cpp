/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The town's desires (docs/bw1-notes/villagers.md): the table, the
// desire functions with the info.dat values, CallDesireFunction, the modifications, the MSVC qsort orders
// (reference: an emulation that runs the original's sort with its comparator), the share-out
// CheckVillagerNeededForTownDesire, TownDesire::Process and the scripts' boosts.

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "3D/SkyType.h"
#include "ECS/Components/Town.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownDesire.h"
#include "Enums.h"
#include "Game/GameStats.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace td = openblack::ecs::town_desire;

namespace
{
constexpr size_t D(TownDesireInfo d)
{
	return static_cast<size_t>(static_cast<int>(d));
}

class TownDesireTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// GTownInfo / GTownDesireInfo of info.dat
		_town.processAbodeEvery = 1;
		_town.maxAlignmentChangePerGameTurnForDesires = 1.0f;
		_town.bedTimeMod = 0.5f;
		_town.foodWantedMultiplier = 5.0f;
		_town.minimumWoodForDesire = 500.0f;
		_town.maximumWoodForDesire = 5000.0f;
		_town.numOfBuildingsForDesiredWood = 10.0f;
		_town.relaxationMod = 2.0f;
		_town.thresholdToStartRepairing = 0.9f;
		_town.gameTurnsAfterEmergencyVillagersReact = 1200;
		_town.divisorForAverageDesires = 5.0f;
		_town.thresholdForAverageDesiresHelpSprites = 0.6f;
		_town.shuffleVillagersEvery = 5000;
		for (auto& info : _info)
		{
			info.showsAfterPercent = 0.1f;
			info.desireTriggersVillagerAction = 0.01f;
			info.tribeMultiplier.fill(1.0f);
		}
		_info.at(D(TownDesireInfo::ForChildren)).desireTriggersVillagerAction = 0.25f;
		_info.at(D(TownDesireInfo::ToBuildWonder)).desireTriggersVillagerAction = 0.75f;
		_info.at(D(TownDesireInfo::ForRelaxation)).desireTriggersVillagerAction = 0.0f;
		auto& food = _info.at(D(TownDesireInfo::ForFood)).tribeMultiplier;
		food.at(static_cast<size_t>(Tribe::CELTIC)) = 1.1f;
		food.at(static_cast<size_t>(Tribe::AZTEC)) = 0.9f;
		food.at(static_cast<size_t>(Tribe::NORSE)) = 1.1f;
		// the campaign's sky thresholds (SetDayNightTimes from the default cycle)
		sky_type::SetThresholds(0.786f, 1.206f, 1.626f, 2.046f);
		_warnings.clear();
		_sink = [this](td::Warning w, float v) { _warnings.emplace_back(w, v); };
	}

	void TearDown() override { sky_type::SetThresholds(4.5f, 7.0f, 7.5f, 8.25f); }

	td::DesireContext Context(bool warn = true)
	{
		return td::DesireContext {_desire, _in, _town, _info, 150, 250, warn ? &_sink : nullptr};
	}

	/// Civic's PopulationWhenNeeded (info.dat: storage pit 0, town centre 0, creche 20, graveyard 40, the rest -1)
	void CivicNeeds()
	{
		_in.populationWhenNeeded.fill(-1);
		_in.populationWhenNeeded.at(static_cast<size_t>(AbodeNumber::StoragePit)) = 0;
		_in.populationWhenNeeded.at(static_cast<size_t>(AbodeNumber::TownCentre)) = 0;
		_in.populationWhenNeeded.at(static_cast<size_t>(AbodeNumber::Creche)) = 20;
		_in.populationWhenNeeded.at(static_cast<size_t>(AbodeNumber::Graveyard)) = 40;
		for (size_t i = 0; i < 6; ++i)
		{
			_in.stats.abodesByNumber.at(i) = 1; // the houses A..F
		}
	}

	static std::string Order(const std::array<DesireSort, td::k_Count>& sorted)
	{
		std::string text;
		for (const auto& e : sorted)
		{
			text += (text.empty() ? "" : " ") + std::to_string(e.index);
		}
		return text;
	}

	static std::string Sorted(const std::array<float, td::k_Count>& values)
	{
		std::array<DesireSort, td::k_Count> entries {};
		for (size_t i = 0; i < td::k_Count; ++i)
		{
			entries.at(i) = {0.0f, values.at(i), static_cast<uint32_t>(i)};
		}
		td::MsvcQsort(entries);
		return Order(entries);
	}

	GTownInfo _town {};
	std::array<GTownDesireInfo, 17> _info {};
	TownDesire _desire {};
	td::DesireInputs _in {};
	td::WarningSink _sink;
	std::vector<std::pair<td::Warning, float>> _warnings;
};
} // namespace

TEST_F(TownDesireTest, Table)
{
	const auto& table = td::Table();
	ASSERT_EQ(table.size(), 17u);
	const std::array<const char*, 17> names = {
	    "Food",           "Wood",         "Playtime", "Protection", "Mercy",   "Abodes",      "Civic_Buildings",
	    "Supply_Worship", "For_Children", "To_Build", "For_Rain",   "For_Sun", "Repair_Town", "Suppy_Workshop",
	    "For_Wonder",     "Relaxation",   "Sleep"};
	for (size_t d = 0; d < 17; ++d)
	{
		EXPECT_STREQ(table.at(d).name, names.at(d)) << d;
		EXPECT_NE(table.at(d).function, nullptr) << d;
		EXPECT_NE(table.at(d).modification, nullptr) << d;
	}
	// CheckSatisfy: 0 1 2 5 6 7 9 12 13 15 16; children: 2 3 4 15 16; Amount / Desired: 5 6 7
	for (const size_t d : {0u, 1u, 2u, 5u, 6u, 7u, 9u, 12u, 13u, 15u, 16u})
	{
		EXPECT_NE(table.at(d).checkSatisfy, nullptr) << d;
	}
	for (const size_t d : {3u, 4u, 8u, 10u, 11u, 14u})
	{
		EXPECT_EQ(table.at(d).checkSatisfy, nullptr) << d;
	}
	for (size_t d = 0; d < 17; ++d)
	{
		const bool children = d == 2 || d == 3 || d == 4 || d == 15 || d == 16;
		EXPECT_EQ(table.at(d).children, children) << d;
		const bool amount = d == 5 || d == 6 || d == 7;
		EXPECT_EQ(table.at(d).amount != nullptr, amount) << d;
		EXPECT_EQ(table.at(d).desired != nullptr, amount) << d;
	}
	EXPECT_EQ(table.at(0).modification, &td::ModificationFood);
	EXPECT_EQ(table.at(1).modification, &td::ModificationWood);
	EXPECT_EQ(table.at(9).modification, &td::ModificationToBuild);
	EXPECT_EQ(table.at(16).modification, &td::ModificationGeneral);
	// FindDesire ignores case
	EXPECT_EQ(td::FindDesire("civic_buildings"), 6);
	EXPECT_EQ(td::FindDesire("ABODES"), 5);
	EXPECT_EQ(td::FindDesire("Sleep"), 16);
	EXPECT_EQ(td::FindDesire("Civic"), std::nullopt);
}

TEST_F(TownDesireTest, Food)
{
	// 10 adults x 85 -> desired 5 x 850 = 4250; storage pit 2000, carried 0
	_in.stats.adults = 10;
	_in.stats.foodForDinner = 850.0f;
	_in.storageFood = 2000;
	_in.isLocalPlayer = true;
	EXPECT_NEAR(td::DesireForFood(Context()), 0.529412f, 1e-5f);
	EXPECT_TRUE(_warnings.empty());
	// no storage pit: the temporary pot
	_in.storageFood.reset();
	_in.potFood = 300;
	EXPECT_NEAR(td::DesireForFood(Context()), 1.0f - 300.0001f / 4250.0001f, 1e-5f);
	// a storage pit wins over the pot; food >= desired -> 0
	_in.storageFood = 5000;
	EXPECT_FLOAT_EQ(td::DesireForFood(Context()), 0.0f);
	// carried food counts (the stats' float truncated)
	_in.storageFood = 0;
	_in.potFood.reset();
	_in.stats.foodCarried = 4250.9f;
	EXPECT_NEAR(td::DesireForFood(Context()), 0.0f, 1e-6f);
	// v >= 0.95 for the local player: one warning with min(v, 2) - 0.9
	_in.stats.foodCarried = 0.0f;
	EXPECT_NEAR(td::DesireForFood(Context()), 1.0f, 1e-6f);
	ASSERT_EQ(_warnings.size(), 1u);
	EXPECT_EQ(_warnings.at(0).first, td::Warning::LowOnFood);
	EXPECT_NEAR(_warnings.at(0).second, 0.1f, 1e-6f);
	// not the local player: none
	_warnings.clear();
	_in.isLocalPlayer = false;
	EXPECT_NEAR(td::DesireForFood(Context()), 1.0f, 1e-6f);
	EXPECT_TRUE(_warnings.empty());
}

TEST_F(TownDesireTest, Wood)
{
	_in.stats.adults = 10;
	_in.abodeCount = 5; // k = max(5 / 10, 1) = 1
	_in.storageWood = 0;
	_in.isLocalPlayer = true;
	// w 0 -> 1 (v = 1.0001) and the warning min(v, 2) - 1
	EXPECT_FLOAT_EQ(td::DesireForWood(Context()), 1.0f);
	ASSERT_EQ(_warnings.size(), 1u);
	EXPECT_EQ(_warnings.at(0).first, td::Warning::LowOnWood);
	EXPECT_NEAR(_warnings.at(0).second, 0.0001f, 1e-6f);
	_warnings.clear();
	_in.storageWood = 250;
	EXPECT_NEAR(td::DesireForWood(Context()), 0.475095f, 1e-6f);
	EXPECT_TRUE(_warnings.empty());
	_in.storageWood = 2500;
	EXPECT_NEAR(td::DesireForWood(Context()), 0.00005f, 1e-7f);
	// 25 abodes -> k 2.5: B 1250, C 12500; w 250: (1 - 0.02) (1 - 0.2 + 0.0001)
	_in.abodeCount = 25;
	_in.storageWood = 250;
	EXPECT_NEAR(td::DesireForWood(Context()), 0.98f * 0.8001f, 1e-6f);
	// R5 + R6 + R9 + R12 = 4 -> S = 3: w 2500, k 1: 0.5 (0 + 3.0001) -> 1 (warning 0.50005)
	_in.abodeCount = 5;
	_in.storageWood = 2500;
	_desire.raw.at(5) = 1.0f;
	_desire.boost.at(6) = 1.0f;
	_desire.boostA.at(9) = 1.0f;
	_desire.raw.at(12) = 1.0f;
	EXPECT_FLOAT_EQ(td::DesireForWood(Context()), 1.0f);
	ASSERT_EQ(_warnings.size(), 1u);
	EXPECT_NEAR(_warnings.at(0).second, 0.50005f, 1e-5f);
}

TEST_F(TownDesireTest, Abodes)
{
	_in.stats.adults = 10;
	_in.stats.adultPlaces = 12;
	_in.stats.children = 3;
	_in.stats.childPlaces = 6;
	const float a = 10.0f / 12.00001f;
	EXPECT_NEAR(td::DesireForAbodes(Context()), a * a * a * a, 1e-5f);
	EXPECT_NEAR(td::DesireForAbodes(Context()), 0.48225f, 1e-4f);
	// R9 = 0.5: half
	_desire.raw.at(9) = 0.5f;
	EXPECT_NEAR(td::DesireForAbodes(Context()), 0.5f * a * a * a * a, 1e-5f);
	// D6 (last turn's, with its boosts) = 0.5: half again
	_desire.boost.at(6) = 0.5f;
	EXPECT_NEAR(td::DesireForAbodes(Context()), 0.25f * a * a * a * a, 1e-5f);
	_desire = {};
	// 15 / 10: min(1.5, 1.5)^4 -> 1
	_in.stats.adults = 15;
	_in.stats.adultPlaces = 10;
	EXPECT_FLOAT_EQ(td::DesireForAbodes(Context()), 1.0f);
	// children above adults: c is taken
	_in.stats.adults = 3;
	_in.stats.adultPlaces = 12;
	_in.stats.children = 5;
	_in.stats.childPlaces = 6;
	const float c = 5.0f / 6.00001f;
	EXPECT_NEAR(td::DesireForAbodes(Context()), c * c * c * c, 1e-5f);
}

TEST_F(TownDesireTest, CivicBuildings)
{
	CivicNeeds();
	_in.stats.adults = 20;
	_in.stats.children = 5;
	_in.homeless = 2;
	// with a storage pit and a town centre: the creche (20 <= 25) counts, the graveyard (40) does not
	_in.stats.abodesByNumber.at(static_cast<size_t>(AbodeNumber::StoragePit)) = 1;
	_in.stats.abodesByNumber.at(static_cast<size_t>(AbodeNumber::TownCentre)) = 1;
	EXPECT_NEAR(td::DesireForCivicBuildings(Context()), 0.625019f, 1e-5f);
	// without the storage pit: (25.001 / 0.001) / 2 + 0.5 -> 1
	_in.stats.abodesByNumber.at(static_cast<size_t>(AbodeNumber::StoragePit)) = 0;
	EXPECT_FLOAT_EQ(td::DesireForCivicBuildings(Context()), 1.0f);
	// homeless 6 (0.24 >= 0.2): only p = 0 counts (the creche no longer)
	_in.stats.abodesByNumber.at(static_cast<size_t>(AbodeNumber::StoragePit)) = 1;
	_in.homeless = 6;
	EXPECT_FLOAT_EQ(td::DesireForCivicBuildings(Context()), 0.0f);
	_in.stats.abodesByNumber.at(static_cast<size_t>(AbodeNumber::TownCentre)) = 0;
	EXPECT_FLOAT_EQ(td::DesireForCivicBuildings(Context()), 1.0f);
	// pop 0: the share is 0 / 0 (unordered: passes) and only p = 0 is <= 0
	_in.stats.adults = 0;
	_in.stats.children = 0;
	_in.homeless = 0;
	_in.stats.abodesByNumber.at(static_cast<size_t>(AbodeNumber::TownCentre)) = 1;
	EXPECT_FLOAT_EQ(td::DesireForCivicBuildings(Context()), 0.0f);
	_in.stats.abodesByNumber.at(static_cast<size_t>(AbodeNumber::TownCentre)) = 0;
	EXPECT_FLOAT_EQ(td::DesireForCivicBuildings(Context()), 1.0f); // (0.001 / 0.001) / 2 + 0.5
}

TEST_F(TownDesireTest, ForChildren)
{
	_desire.raw.at(0) = 0.4f; // A = 0.6
	_in.stats.children = 2;   // < 4: B = 1
	_in.stats.childPlaces = 4;
	_in.stats.adults = 10; // < 12: C = 1
	_in.stats.adultPlaces = 12;
	_desire.desire.at(3) = 0.2f; // P = 0.2, M = 0: Q = 0.8
	_in.crecheFunctional = true;
	_in.alignment = 0.0f;
	EXPECT_NEAR(td::DesireForChildren(Context()), 0.48f, 1e-6f);
	_in.alignment = 1.0f; // al = 0.5
	EXPECT_NEAR(td::DesireForChildren(Context()), 0.72f, 1e-6f);
	_in.crecheFunctional = false; // x 0.5
	EXPECT_NEAR(td::DesireForChildren(Context()), 0.36f, 1e-6f);
	_in.alignment = 0.0f;
	EXPECT_NEAR(td::DesireForChildren(Context()), 0.24f, 1e-6f);
	// adults >= places: C = 0.5
	_in.stats.adults = 12;
	EXPECT_NEAR(td::DesireForChildren(Context()), 0.12f, 1e-6f);
	// R0 <= 0: A = 1; R0 >= 1: 0
	_desire.raw.at(0) = -0.2f;
	EXPECT_NEAR(td::DesireForChildren(Context()), 0.2f, 1e-6f);
	_desire.raw.at(0) = 1.0f;
	EXPECT_FLOAT_EQ(td::DesireForChildren(Context()), 0.0f);
	// no player: al 0 and TribalPower 1
	_desire.raw.at(0) = 0.4f;
	_in.owner = std::nullopt;
	_in.alignment = 1.0f;
	_in.tribalPower4 = 3.0f;
	EXPECT_NEAR(td::DesireForChildren(Context()), 0.12f, 1e-6f);
}

TEST_F(TownDesireTest, Playtime)
{
	_in.turn = 4000;
	EXPECT_FLOAT_EQ(td::DesireForPlaytime(Context()), 0.0f);
	_in.turn = 4001;
	EXPECT_FLOAT_EQ(td::DesireForPlaytime(Context()), 0.1f);
	// one of D0, D1, D5, D6, D9 at its trigger (not strictly below): 0
	_desire.desire.at(9) = 0.01f;
	EXPECT_FLOAT_EQ(td::DesireForPlaytime(Context()), 0.0f);
	_desire.desire.at(9) = 0.0f;
	_desire.boost.at(0) = 0.5f;
	EXPECT_FLOAT_EQ(td::DesireForPlaytime(Context()), 0.0f);
}

TEST_F(TownDesireTest, Sleep)
{
	const std::vector<std::pair<float, float>> curve = {
	    {12.0f, 0.0f}, {21.0f, 0.0f},     {22.0f, 0.371519f}, {22.5f, 2.25f}, {23.5f, 6.25f},
	    {0.5f, 2.25f}, {1.0f, 0.981043f}, {1.5f, 0.25f},      {3.0f, 0.0f},
	};
	for (const auto& [hour, expected] : curve)
	{
		_in.visualHour = hour;
		EXPECT_NEAR(td::DesireForSleep(Context()), expected, 1e-4f) << hour;
	}
}

TEST_F(TownDesireTest, Relaxation)
{
	const std::vector<std::pair<float, float>> curve = {{12.0f, 0.1f}, {20.5f, 0.546f}, {21.5f, 1.0f}, {22.5f, 0.1f}};
	for (const auto& [hour, expected] : curve)
	{
		_in.visualHour = hour;
		EXPECT_NEAR(td::DesireForRelaxation(Context()), expected, 1e-4f) << hour;
	}
}

TEST_F(TownDesireTest, Repair)
{
	td::RepairInput house {0.5f, true, 3, 0.5f};
	EXPECT_NEAR(td::AbodeDesireToBeRepaired(house, _town), 0.375f, 1e-6f);
	house.life = 0.95f; // above thresholdToStartRepairing 0.9
	EXPECT_FLOAT_EQ(td::AbodeDesireToBeRepaired(house, _town), 0.0f);
	house.life = 0.5f;
	house.inhabitants = 0; // a home with nobody
	EXPECT_FLOAT_EQ(td::AbodeDesireToBeRepaired(house, _town), 0.0f);
	td::RepairInput store {0.5f, false, 0, 0.5f}; // not a home: counts empty
	EXPECT_NEAR(td::AbodeDesireToBeRepaired(store, _town), 0.375f, 1e-6f);
	// two abodes: the sum, at most 1
	_in.abodes = {{0.5f, true, 2, 0.5f}, {0.5f, true, 1, 0.5f}};
	EXPECT_NEAR(td::DesireToRepair(Context()), 0.75f, 1e-6f);
	_in.abodes = {{0.5f, true, 2, 1.0f}, {0.5f, true, 1, 1.0f}};
	EXPECT_FLOAT_EQ(td::DesireToRepair(Context()), 1.0f);
}

TEST_F(TownDesireTest, CallDesireFunction)
{
	// Food of a Celtic town: f 0.529412 x 1.1 into the raw; the desire is raw x the food modification
	_in.stats.adults = 10;
	_in.stats.foodForDinner = 850.0f;
	_in.storageFood = 2000;
	_in.tribe = Tribe::CELTIC;
	const float f = 1.0f - 2000.0001f / 4250.0001f;
	const float desire = td::CallDesireFunction(_desire, Context(), 0);
	EXPECT_NEAR(_desire.raw.at(0), 1.1f * f, 1e-5f);
	EXPECT_NEAR(desire, 1.1f * f * (1.0f - 2000.0001f / 4250.0001f), 1e-5f);
	// the raw is not clamped, the desire is: Sleep at 23.5 -> raw 6.25, desire 1 (nobody serving it)
	_in.visualHour = 23.5f;
	_desire.desire.at(16) = td::CallDesireFunction(_desire, Context(), 16); // (ProcessDesire stores it)
	EXPECT_FLOAT_EQ(_desire.desire.at(16), 1.0f);
	EXPECT_FLOAT_EQ(_desire.raw.at(16), 6.25f);
	// a negative boost does not enter the raw (GetRawDesire adds it)
	_desire.boost.at(16) = -0.75f;
	EXPECT_FLOAT_EQ(td::GetRawDesire(_desire, 16), 5.5f);
	EXPECT_FLOAT_EQ(td::GetDesire(_desire, 16), 0.25f);
}

TEST_F(TownDesireTest, Modifications)
{
	// general: doingNow 3, population 10 -> 0.7
	_in.stats.adults = 8;
	_in.stats.children = 2;
	_desire.doingNow.at(16) = 3.0f;
	EXPECT_NEAR(td::ModificationGeneral(Context(), 16), 0.7f, 1e-5f);
	_desire.doingNow.at(16) = 30.0f;
	EXPECT_FLOAT_EQ(td::ModificationGeneral(Context(), 16), 0.0f);
	// food: 150 x doingNow + the storage pit's food (no pot) over the desired food
	_in.stats.foodForDinner = 850.0f;
	_in.storageFood = 2000;
	_desire.doingNow.at(0) = 10.0f;
	EXPECT_NEAR(td::ModificationFood(Context(), 0), 1.0f - 3500.0001f / 4250.0001f, 1e-5f);
	_in.storageFood.reset();
	_in.potFood = 2000; // the pot is not read
	EXPECT_NEAR(td::ModificationFood(Context(), 0), 1.0f - 1500.0001f / 4250.0001f, 1e-5f);
	// wood: 250 x doingNow + the store's wood over the maximum wood (5000 with k 1)
	_in.storageWood = 1000;
	_desire.doingNow.at(1) = 4.0f;
	EXPECT_NEAR(td::ModificationWood(Context(), 1), 1.0f - 2000.0001f / 5000.0001f, 1e-5f);
	// To_Build without building sites: 1 - 1e-4 / 1e-4 = 0
	EXPECT_FLOAT_EQ(td::ModificationToBuild(Context(), 9), 0.0f);
	_in.siteBuilders = {1};
	_in.sitePlaces = {4};
	EXPECT_NEAR(td::ModificationToBuild(Context(), 9), 1.0f - 1.0001f / 4.0001f, 1e-5f);
	// the temporary one: only what started this turn
	_desire.doingNow.at(5) = 5.0f;
	_desire.doingNowAtStart.at(5) = 3.0f;
	EXPECT_NEAR(td::GetTemporaryDesireVillagerModification(_desire, 10, 5), 0.8f, 1e-5f);
	_desire.doingNowAtStart.at(5) = 6.0f; // fewer than at the start: 0
	EXPECT_FLOAT_EQ(td::GetTemporaryDesireVillagerModification(_desire, 10, 5), 1.0f);
}

TEST_F(TownDesireTest, QsortOrders)
{
	// the orders of the original's qsort with its comparator, emulated
	std::array<float, 17> zero {};
	EXPECT_EQ(Sorted(zero), "8 1 2 3 4 5 6 7 0 9 10 11 12 13 14 15 16");
	std::array<float, 17> a {};
	a.at(0) = 0.3f;
	a.at(1) = 1.0f;
	a.at(8) = 0.2f;
	a.at(15) = 0.1f;
	EXPECT_EQ(Sorted(a), "1 0 8 15 5 6 7 2 4 9 3 11 12 13 14 10 16");
	std::array<float, 17> b {};
	b.at(15) = 0.1f;
	b.at(16) = 1.0f;
	EXPECT_EQ(Sorted(b), "16 15 3 4 5 6 2 1 7 9 10 11 12 13 14 0 8");
	// the emulation's input: 0.5 -0.2 0.1 0.1 0 0.9 -1 0.1 0.25 0.5 0 0 0.3 0 0.75 0.1 6.25
	const std::array<float, 17> c = {0.5f, -0.2f, 0.1f, 0.1f, 0.0f, 0.9f,  -1.0f, 0.1f, 0.25f,
	                                 0.5f, 0.0f,  0.0f, 0.3f, 0.0f, 0.75f, 0.1f,  6.25f};
	EXPECT_EQ(Sorted(c), "16 5 14 9 0 12 8 3 7 15 2 11 13 10 4 1 6");
	// the +0 of each entry: order 1 = BoostA + Boost, order 2 = BoostA; order 2 sorts the raw
	_desire.boostA.at(3) = 0.25f;
	_desire.boost.at(3) = 0.5f;
	_desire.raw.at(16) = 6.25f;
	_desire.desire.at(16) = 1.0f;
	td::SortDesires(_desire);
	td::SortRawDesires(_desire);
	EXPECT_EQ(_desire.sorted.at(0).index, 16u);
	EXPECT_EQ(_desire.sorted.at(1).index, 3u);
	EXPECT_FLOAT_EQ(_desire.sorted.at(1).boosts, 0.75f);
	EXPECT_FLOAT_EQ(_desire.sorted.at(1).value, 0.75f);
	EXPECT_FLOAT_EQ(_desire.sortedRaw.at(0).value, 6.25f);
	EXPECT_EQ(_desire.sortedRaw.at(1).index, 3u);
	EXPECT_FLOAT_EQ(_desire.sortedRaw.at(1).boosts, 0.25f);
	EXPECT_EQ(td::GetMostSignificantRawDesire(_desire, 6.0f), 16);
	EXPECT_EQ(td::GetMostSignificantRawDesire(_desire, 7.0f), std::nullopt);
	EXPECT_EQ(td::GetMostDesired(_desire), 16); // the desire only, no boosts
}

TEST_F(TownDesireTest, ShareOut)
{
	std::vector<size_t> asked;
	uint32_t answer = 0;
	const auto cs = [&](size_t d) {
		asked.push_back(d);
		return answer;
	};
	// [Sleep 1 (16), Food 0.5 (0)], nobody serving: Sleep's CheckSatisfy, then Food's
	_desire.sorted.at(0) = {0.0f, 1.0f, 16};
	_desire.sorted.at(1) = {0.0f, 0.5f, 0};
	for (size_t k = 2; k < 17; ++k)
	{
		_desire.sorted.at(k) = {0.0f, 0.0f, static_cast<uint32_t>(k == 16 ? 1 : k)};
	}
	EXPECT_EQ(td::CheckVillagerNeeded(_desire, _info, 10, false, 0.3f, cs), 0u);
	EXPECT_EQ(asked, (std::vector<size_t> {16, 0})); // then the cut at the third (0 <= t)
	// a CheckSatisfy that takes it: 1 at once
	asked.clear();
	answer = 1;
	EXPECT_EQ(td::CheckVillagerNeeded(_desire, _info, 10, false, 0.3f, cs), 1u);
	EXPECT_EQ(asked, (std::vector<size_t> {16}));
	// a child skips Food (not for children) without cutting
	asked.clear();
	answer = 0;
	EXPECT_EQ(td::CheckVillagerNeeded(_desire, _info, 10, true, 0.3f, cs), 0u);
	EXPECT_EQ(asked, (std::vector<size_t> {16}));
	// an entry without CheckSatisfy does not cut (For_Children 8 on top with 0)
	asked.clear();
	answer = 1;
	_desire.sorted.at(0) = {0.0f, 0.0f, 8};
	_desire.sorted.at(1) = {0.0f, 1.0f, 16};
	EXPECT_EQ(td::CheckVillagerNeeded(_desire, _info, 10, false, 0.3f, cs), 1u);
	EXPECT_EQ(asked, (std::vector<size_t> {16}));
	// the cut: value <= t = min(trigger + desireTriggersVillagerAction, 1), t stored as a float (0.29f + 0.01f = 0.29999998f)
	asked.clear();
	const auto t = static_cast<float>(static_cast<double>(0.29f) + 0.01f);
	_desire.sorted.at(0) = {0.0f, t, 16};
	EXPECT_EQ(td::CheckVillagerNeeded(_desire, _info, 10, false, 0.29f, cs), 0u);
	EXPECT_TRUE(asked.empty());
	_desire.sorted.at(0) = {0.0f, 0.3f, 16}; // 0.3f is above 0.29999998f: not cut
	EXPECT_EQ(td::CheckVillagerNeeded(_desire, _info, 10, false, 0.29f, cs), 1u);
	EXPECT_EQ(asked, (std::vector<size_t> {16}));
	asked.clear();
	_desire.sorted.at(0) = {0.0f, 1.0f, 16};
	EXPECT_EQ(td::CheckVillagerNeeded(_desire, _info, 10, false, 0.995f, cs), 0u); // t min(1.005, 1)
	EXPECT_TRUE(asked.empty());
	// trigger 0 -> 0.001: t 0.011
	_desire.sorted.at(0) = {0.0f, 0.0105f, 16};
	EXPECT_EQ(td::CheckVillagerNeeded(_desire, _info, 10, false, 0.0f, cs), 0u);
	EXPECT_TRUE(asked.empty());
	EXPECT_EQ(td::CheckVillagerNeeded(_desire, _info, 10, false, 0.000001f, cs), 1u);
	EXPECT_EQ(asked, (std::vector<size_t> {16}));
	// the oddity: [Sleep 1 (16), Food 0.5 (0)] with 10 villagers starting Food this turn (doingNow[0] - copy[0] = 10,
	// pop 10): GetTemporaryDesireVillagerModification(k = 0) is Food's, 1e-6, so Sleep is cut though nobody serves it
	asked.clear();
	_desire.sorted.at(0) = {0.0f, 1.0f, 16};
	_desire.sorted.at(1) = {0.0f, 0.5f, 0};
	_desire.doingNow.at(0) = 10.0f;
	EXPECT_EQ(td::CheckVillagerNeeded(_desire, _info, 10, false, 0.3f, cs), 0u);
	EXPECT_TRUE(asked.empty());
	// ... and Sleep's own counters do not matter at position 0
	_desire.doingNow.at(0) = 0.0f;
	_desire.doingNow.at(16) = 10.0f;
	EXPECT_EQ(td::CheckVillagerNeeded(_desire, _info, 10, false, 0.3f, cs), 1u);
	EXPECT_EQ(asked, (std::vector<size_t> {16}));
}

TEST_F(TownDesireTest, Process)
{
	_in.stats.adults = 10;
	_in.stats.adultPlaces = 12;
	_in.stats.children = 3;
	_in.stats.childPlaces = 6;
	_in.stats.foodForDinner = 850.0f;
	_in.storageFood = 4250;
	_in.storageWood = 5000;
	_in.isLocalPlayer = true;
	_in.worshipping = 2;
	_in.onWayToWorship = 1;
	_in.turn = 51;
	_in.visualHour = 12.0f;
	CivicNeeds();
	_in.stats.abodesByNumber.at(static_cast<size_t>(AbodeNumber::StoragePit)) = 1;
	_in.stats.abodesByNumber.at(static_cast<size_t>(AbodeNumber::TownCentre)) = 1;
	_in.stats.abodesByNumber.at(static_cast<size_t>(AbodeNumber::Creche)) = 1;
	// last turn's Civic desire 0.5: Abodes (5) reads it before Civic (6) is done this turn
	_desire.desire.at(6) = 0.5f;
	_desire.doingNow.at(2) = -1.0f;
	_desire.doingNow.at(16) = 2.0f;
	_desire.doingNowCount.at(16) = 1.0f;
	const float a = 10.0f / 12.00001f;
	EXPECT_FALSE(td::Process(_desire, Context()).has_value()); // turn 51: no average
	EXPECT_FLOAT_EQ(_desire.population, 10.0f);                // 10 + 3 - 1 - 2
	EXPECT_NEAR(_desire.raw.at(5), 0.5f * a * a * a * a, 1e-5f);
	EXPECT_FLOAT_EQ(_desire.desire.at(6), 0.0f); // the creche's there: nothing needed
	EXPECT_FLOAT_EQ(_desire.doingNow.at(2), 0.0f);
	EXPECT_FLOAT_EQ(_desire.doingNowAtStart.at(16), 2.0f);
	EXPECT_FLOAT_EQ(_desire.doingNowCountAtStart.at(16), 1.0f);
	EXPECT_FLOAT_EQ(_desire.desire.at(15), 0.1f * (1.0f - 0.0f)); // Relaxation by day
	EXPECT_FLOAT_EQ(_desire.amount.at(5), 0.0f);                  // abodesWithPlaces 0 in the stats
	// For_Children (no working creche: x 0.5) 0.5, Abodes 0.24, Relaxation 0.1
	EXPECT_EQ(_desire.sorted.at(0).index, 8u);
	EXPECT_NEAR(_desire.sorted.at(0).value, 0.5f, 1e-6f);
	EXPECT_EQ(_desire.sorted.at(1).index, 5u);
	EXPECT_EQ(_desire.sorted.at(2).index, 15u);
	EXPECT_TRUE(_warnings.empty());
	// every 50 turns: (2 R0 + R1 + max(R3, R4) + max(R5, R6)) / 5 and the warning above 0.6 for the local player
	_in.turn = 50;
	_desire.boost.at(0) = 1.5f;
	_in.owner = PlayerNames::PLAYER_TWO;
	const auto before = openblack::game_stats::Of(1);
	const auto average = td::Process(_desire, Context());
	ASSERT_TRUE(average.has_value());
	// the owner's GameStats desireCount ++, desireSum += 1 - avg
	EXPECT_EQ(openblack::game_stats::Of(1).desireCount, before.desireCount + 1);
	EXPECT_FLOAT_EQ(openblack::game_stats::Of(1).desireSum, before.desireSum + (1.0f - *average));
	const float expected = (2.0f * td::GetRawDesire(_desire, 0) + td::GetRawDesire(_desire, 1) +
	                        std::max(td::GetRawDesire(_desire, 3), td::GetRawDesire(_desire, 4)) +
	                        std::max(td::GetRawDesire(_desire, 5), td::GetRawDesire(_desire, 6))) /
	                       5.0f;
	EXPECT_NEAR(*average, expected, 1e-5f);
	EXPECT_GT(*average, 0.6f);
	ASSERT_EQ(_warnings.size(), 1u);
	EXPECT_EQ(_warnings.at(0).first, td::Warning::VillagersUnhappy);
	// not the local player: no warning
	_warnings.clear();
	_in.isLocalPlayer = false;
	EXPECT_TRUE(td::Process(_desire, Context()).has_value());
	EXPECT_TRUE(_warnings.empty());
}

TEST_F(TownDesireTest, NoPlayerNoAverageAndNoGameStatsSample)
{
	// the average turn, but a town without a player: no average, so no GameStats sample
	_in.turn = 50;
	_in.owner = std::nullopt;
	const auto before = openblack::game_stats::Of(1);
	EXPECT_FALSE(td::Process(_desire, Context()).has_value());
	EXPECT_EQ(openblack::game_stats::Of(1).desireCount, before.desireCount);
}

TEST_F(TownDesireTest, Scripts)
{
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	openblack::test::EmplaceWorldSystems();
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = registry.Create();
	registry.Assign<Town>(town, 7u);
	auto& t = registry.Get<Town>(town);
	t.desire.desire.at(16) = 0.2f;
	td::SortDesires(t.desire);
	td::SortRawDesires(t.desire);
	ASSERT_EQ(t.desire.sorted.at(0).index, 16u);
	std::vector<std::string> errors;
	// SET_TOWN_DESIRE_BOOST: the boost and order 1 again, not order 2
	EXPECT_TRUE(td::ScriptSetTownDesireBoost(town, 5, 0.75f, &errors));
	EXPECT_TRUE(errors.empty());
	EXPECT_FLOAT_EQ(t.desire.boost.at(5), 0.75f);
	EXPECT_EQ(t.desire.sorted.at(0).index, 5u);
	EXPECT_FLOAT_EQ(t.desire.sortedRaw.at(0).value, 0.0f);
	EXPECT_NE(t.desire.sortedRaw.at(0).index, 5u);
	// out of range: nothing written, "Invalid Params"
	EXPECT_FALSE(td::ScriptSetTownDesireBoost(town, 5, 1.5f, &errors));
	EXPECT_FALSE(td::ScriptSetTownDesireBoost(town, 17, 0.5f, &errors));
	EXPECT_FALSE(td::ScriptSetTownDesireBoost(town, 5, -1.01f, &errors));
	EXPECT_EQ(errors, (std::vector<std::string> {"Invalid Params", "Invalid Params", "Invalid Params"}));
	EXPECT_FLOAT_EQ(t.desire.boost.at(5), 0.75f);
	// not a town: "Thing not valid!"
	errors.clear();
	EXPECT_FALSE(td::ScriptSetTownDesireBoost(entt::null, 5, 0.5f, &errors));
	EXPECT_EQ(errors, (std::vector<std::string> {"Thing not valid!"}));
	// TOWN_DESIRE_BOOST "civic_buildings": boost[6], no re-sort, no range check
	td::MapTownDesireBoost(town, "civic_buildings", -0.75f);
	EXPECT_FLOAT_EQ(t.desire.boost.at(6), -0.75f);
	td::MapTownDesireBoost(town, "Abodes", 3.0f);
	EXPECT_FLOAT_EQ(t.desire.boost.at(5), 3.0f);
	EXPECT_FLOAT_EQ(t.desire.sorted.at(0).value, 0.75f); // not re-sorted
	td::MapTownDesireBoost(town, "Civic", 1.0f);         // unknown: nothing
	EXPECT_FLOAT_EQ(t.desire.boost.at(6), -0.75f);
	// GET_DESIRE: an invalid d does not pop the object
	errors.clear();
	bool popped = false;
	const auto pop = [&] {
		popped = true;
		return town;
	};
	EXPECT_FLOAT_EQ(td::ScriptGetDesire(17, pop, &errors), 0.0f);
	EXPECT_FALSE(popped);
	EXPECT_EQ(errors, (std::vector<std::string> {"Invalid desire"}));
	EXPECT_FLOAT_EQ(td::ScriptGetDesire(-1, pop, &errors), 0.0f);
	EXPECT_FALSE(popped);
	// a valid d: GetRawDesire (raw + boost + boost A)
	t.desire.raw.at(6) = 1.0f;
	EXPECT_FLOAT_EQ(td::ScriptGetDesire(6, pop, &errors), 0.25f);
	EXPECT_TRUE(popped);
	// not a town: 0 and no message
	errors.clear();
	const auto other = registry.Create();
	EXPECT_FLOAT_EQ(td::ScriptGetDesire(
	                    6, [&] { return other; }, &errors),
	                0.0f);
	EXPECT_TRUE(errors.empty());
	EXPECT_FLOAT_EQ(td::ScriptGetDesire(
	                    6, [] { return entt::entity {entt::null}; }, &errors),
	                0.0f);
	EXPECT_EQ(errors, (std::vector<std::string> {"Object no longer valid"}));
	// the readers
	EXPECT_FLOAT_EQ(td::GetField(town, TownDesireInfo::ForCivicBuilding, td::Field::Boost), -0.75f);
	EXPECT_FLOAT_EQ(td::GetField(town, TownDesireInfo::ForCivicBuilding, td::Field::Raw), 1.0f);
	EXPECT_FLOAT_EQ(td::GetRawDesire(town, TownDesireInfo::ForCivicBuilding), 0.25f);
	td::SetBoost(town, TownDesireInfo::ForSleep, 0.5f, true);
	EXPECT_EQ(td::GetSortedDesires(town).at(0).index, 5u); // 3.0 boost on 5 still first
	Locator::entitiesRegistry::reset();
	openblack::test::ResetWorldSystems();
}
