/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>
#include <set>
#include <vector>

#include <gtest/gtest.h>
#include <landgen/Features.h>
#include <landgen/Generator.h>
#include <landgen/Settlement.h>

using namespace openblack::landgen;
using settlement::Building;
using settlement::Plan;

namespace
{
constexpr float k_StartX = 60.0f;
constexpr float k_StartZ = 70.0f;

Grid<uint8_t> Clear()
{
	return {129, 0};
}

float Distance(float ax, float az, float bx, float bz)
{
	return std::sqrt(((ax - bx) * (ax - bx)) + ((az - bz) * (az - bz)));
}

size_t CountOf(const Plan& plan, Building building)
{
	return static_cast<size_t>(
	    std::ranges::count_if(plan.buildings, [building](const auto& placed) { return placed.building == building; }));
}

size_t HousesOf(const Plan& plan)
{
	return static_cast<size_t>(std::ranges::count_if(plan.buildings, [](const auto& placed) {
		return std::ranges::find(settlement::k_Houses, placed.building) != settlement::k_Houses.end();
	}));
}

features::Feature TreeAt(float x, float z)
{
	return {.kind = features::Kind::Tree, .type = 0, .x = x, .z = z, .angle = 0.0f, .size = 1.0f, .maxSize = 1.0f};
}

/// A small land that generates quickly
Settings SmallSettings(uint64_t seed)
{
	Settings settings;
	settings.seed = seed;
	settings.side = 129;
	settings.base.seaMargin = 12;
	settings.base.hollows = 2;
	settings.base.hollowRadius = 6.0f;
	settings.hydraulic.drops = 8000;
	settings.thermal.passes = 10;
	settings.rivers.sourceFlow = 150.0f;
	settings.wind = climate::Wind::West;
	return settings;
}
} // namespace

TEST(LandgenSettlement, TheTempleAndTheTownAreOnTwoSidesOfTheStart)
{
	const settlement::Settings settings;
	const auto plan = settlement::Make(k_StartX, k_StartZ, Clear(), 7, settings);
	const float middleX = k_StartX * 10.0f;
	const float middleZ = k_StartZ * 10.0f;
	EXPECT_FLOAT_EQ(Distance(plan.citadelX, plan.citadelZ, middleX, middleZ), settings.citadelDistance);
	EXPECT_FLOAT_EQ(Distance(plan.townX, plan.townZ, middleX, middleZ), settings.townDistance);
	EXPECT_FLOAT_EQ(Distance(plan.citadelX, plan.citadelZ, plan.townX, plan.townZ),
	                settings.citadelDistance + settings.townDistance);
	EXPECT_GE(plan.citadelRotation, 0);
	EXPECT_LT(plan.citadelRotation, 6283);
}

TEST(LandgenSettlement, TheTownHasEachBuildingOfAStart)
{
	const settlement::Settings settings;
	for (uint64_t seed = 1; seed <= 20; ++seed)
	{
		const auto plan = settlement::Make(k_StartX, k_StartZ, Clear(), seed, settings);
		ASSERT_FALSE(plan.buildings.empty());
		// The town centre is first, at the place of the town
		EXPECT_EQ(plan.buildings.front().building, Building::TownCentre);
		EXPECT_EQ(plan.buildings.front().x, plan.townX);
		EXPECT_EQ(plan.buildings.front().z, plan.townZ);
		EXPECT_EQ(CountOf(plan, Building::TownCentre), 1u);
		EXPECT_EQ(CountOf(plan, Building::StoragePit), 1u);
		EXPECT_EQ(CountOf(plan, Building::Creche), 1u);
		EXPECT_GE(HousesOf(plan), settings.fewestHouses);
		EXPECT_LE(HousesOf(plan), settings.mostHouses);
		EXPECT_TRUE(std::ranges::find(settlement::k_Tribes, plan.tribe) != settlement::k_Tribes.end());
	}
}

TEST(LandgenSettlement, TheSeedPicksEachTribe)
{
	const settlement::Settings settings;
	std::set<settlement::Tribe> tribes;
	for (uint64_t seed = 1; seed <= 200; ++seed)
	{
		tribes.insert(settlement::Make(k_StartX, k_StartZ, Clear(), seed, settings).tribe);
	}
	EXPECT_EQ(tribes.size(), settlement::k_Tribes.size());
}

TEST(LandgenSettlement, EachHouseHasItsAdults)
{
	const settlement::Settings settings;
	const auto plan = settlement::Make(k_StartX, k_StartZ, Clear(), 11, settings);
	const auto houses = HousesOf(plan);
	EXPECT_GE(plan.villagers.size(), houses * settings.fewestPeople);
	EXPECT_LE(plan.villagers.size(), houses * settings.mostPeople);
	const auto foresters =
	    std::ranges::count_if(plan.villagers, [](const auto& person) { return person.kind == settlement::Villager::Forester; });
	EXPECT_GT(foresters, 0);
	EXPECT_LT(static_cast<size_t>(foresters), plan.villagers.size());
	for (const auto& person : plan.villagers)
	{
		EXPECT_GE(person.age, settings.youngest);
		EXPECT_LE(person.age, settings.oldest);
	}
}

TEST(LandgenSettlement, TheBuildingsDoNotTouch)
{
	const settlement::Settings settings;
	const auto plan = settlement::Make(k_StartX, k_StartZ, Clear(), 3, settings);
	const float nearest = settings.buildingSpacing - (2.0f * settings.buildingShift);
	for (size_t i = 0; i < plan.buildings.size(); ++i)
	{
		for (size_t j = i + 1; j < plan.buildings.size(); ++j)
		{
			EXPECT_GE(Distance(plan.buildings[i].x, plan.buildings[i].z, plan.buildings[j].x, plan.buildings[j].z),
			          nearest - 0.001f);
		}
		// No building is on the side of the temple
		EXPECT_GE(Distance(plan.buildings[i].x, plan.buildings[i].z, plan.citadelX, plan.citadelZ),
		          settings.citadelDistance + settings.townDistance - settings.buildingShift * 2.0f);
	}
}

TEST(LandgenSettlement, TheFieldsAreSquaresOfTheTenMetreLines)
{
	const settlement::Settings settings;
	const auto plan = settlement::Make(k_StartX, k_StartZ, Clear(), 5, settings);
	ASSERT_EQ(plan.fields.size(), static_cast<size_t>(settings.fieldRows * settings.fieldColumns));
	std::set<std::pair<float, float>> places;
	for (const auto& field : plan.fields)
	{
		EXPECT_FLOAT_EQ(std::fmod(field.x, 10.0f), 5.0f);
		EXPECT_FLOAT_EQ(std::fmod(field.z, 10.0f), 5.0f);
		places.insert({field.x, field.z});
		// The fields are past the houses, away from the temple
		EXPECT_GT(Distance(field.x, field.z, plan.citadelX, plan.citadelZ),
		          Distance(plan.townX, plan.townZ, plan.citadelX, plan.citadelZ) + settings.buildingSpacing);
	}
	EXPECT_EQ(places.size(), plan.fields.size());
}

TEST(LandgenSettlement, NothingGoesOnGroundThatTheLandKeepsClear)
{
	const settlement::Settings settings;
	auto keepClear = Clear();
	// A river along the far side of the town and its fields
	for (int x = 0; x < keepClear.side; ++x)
	{
		for (int z = 0; z < keepClear.side; ++z)
		{
			if (std::abs(x - static_cast<int>(k_StartX)) > 6 || std::abs(z - static_cast<int>(k_StartZ)) > 6)
			{
				keepClear.At(x, z) = 1;
			}
		}
	}
	const auto plan = settlement::Make(k_StartX, k_StartZ, keepClear, 9, settings);
	const auto free = [&keepClear](float x, float z) {
		return keepClear.At(static_cast<int>((x / 10.0f) + 0.5f), static_cast<int>((z / 10.0f) + 0.5f)) == 0;
	};
	for (const auto& placed : plan.buildings)
	{
		EXPECT_TRUE(free(placed.x, placed.z));
	}
	for (const auto& field : plan.fields)
	{
		EXPECT_TRUE(free(field.x, field.z));
	}
	EXPECT_LT(plan.fields.size(), static_cast<size_t>(settings.fieldRows * settings.fieldColumns));
}

TEST(LandgenSettlement, TheSameSeedGivesTheSamePlan)
{
	const settlement::Settings settings;
	const auto first = settlement::Make(k_StartX, k_StartZ, Clear(), 42, settings);
	const auto second = settlement::Make(k_StartX, k_StartZ, Clear(), 42, settings);
	ASSERT_EQ(first.buildings.size(), second.buildings.size());
	for (size_t i = 0; i < first.buildings.size(); ++i)
	{
		EXPECT_EQ(first.buildings[i].building, second.buildings[i].building);
		EXPECT_EQ(first.buildings[i].x, second.buildings[i].x);
		EXPECT_EQ(first.buildings[i].z, second.buildings[i].z);
		EXPECT_EQ(first.buildings[i].rotation, second.buildings[i].rotation);
	}
	EXPECT_EQ(first.villagers.size(), second.villagers.size());
	EXPECT_EQ(first.tribe, second.tribe);

	// Other seeds give other towns
	bool differs = false;
	for (uint64_t seed = 43; seed < 48 && !differs; ++seed)
	{
		const auto other = settlement::Make(k_StartX, k_StartZ, Clear(), seed, settings);
		differs = other.citadelX != first.citadelX || other.tribe != first.tribe ||
		          other.buildings.size() != first.buildings.size() || other.buildings[1].x != first.buildings[1].x;
	}
	EXPECT_TRUE(differs);
}

TEST(LandgenForests, ASquareWithEnoughTreesIsAForest)
{
	const features::ForestSettings settings {.square = 100.0f, .fewest = 3, .most = 10};
	std::vector<features::Feature> features {
	    TreeAt(10.0f, 10.0f),  TreeAt(20.0f, 20.0f),  TreeAt(30.0f, 30.0f), // a forest in the first square
	    TreeAt(510.0f, 10.0f), TreeAt(520.0f, 20.0f),                       // too few trees
	};
	features.push_back(
	    {.kind = features::Kind::Static, .type = 3, .x = 40.0f, .z = 40.0f, .angle = 0.0f, .size = 1.0f, .maxSize = 1.0f});
	const auto forests = features::GroupForests(features, 0.0f, 0.0f, settings);
	ASSERT_EQ(forests.size(), 1u);
	EXPECT_EQ(forests[0].id, 1u);
	// The forest is at its first tree
	EXPECT_EQ(forests[0].x, 10.0f);
	EXPECT_EQ(forests[0].z, 10.0f);
	EXPECT_EQ(features[0].forest, 1u);
	EXPECT_EQ(features[2].forest, 1u);
	EXPECT_EQ(features[3].forest, 0u);
	EXPECT_EQ(features[5].forest, 0u);
}

TEST(LandgenForests, TheNearestSquaresMakeTheFirstForests)
{
	const features::ForestSettings settings {.square = 100.0f, .fewest = 2, .most = 2};
	std::vector<features::Feature> features;
	for (const float x : {950.0f, 50.0f, 450.0f})
	{
		features.push_back(TreeAt(x, 50.0f));
		features.push_back(TreeAt(x + 10.0f, 60.0f));
	}
	const auto forests = features::GroupForests(features, 400.0f, 50.0f, settings);
	ASSERT_EQ(forests.size(), 2u);
	// The square at 450 is nearest the point, then the square at 50. The square at 950 is past the most.
	EXPECT_EQ(features[4].forest, 1u);
	EXPECT_EQ(features[2].forest, 2u);
	EXPECT_EQ(features[0].forest, 0u);
}

TEST(LandgenGenerator, TheLandHasItsTownAndForests)
{
	const auto land = Generate(SmallSettings(77));
	const float middleX = land.start.x * 10.0f;
	const float middleZ = land.start.z * 10.0f;
	EXPECT_LT(Distance(land.settlement.townX, land.settlement.townZ, middleX, middleZ), land.start.radius * 10.0f);
	EXPECT_LT(Distance(land.settlement.citadelX, land.settlement.citadelZ, middleX, middleZ), land.start.radius * 10.0f);
	EXPECT_GE(HousesOf(land.settlement), 1u);
	std::set<uint32_t> ids;
	for (const auto& forest : land.forests)
	{
		EXPECT_TRUE(ids.insert(forest.id).second);
	}
	EXPECT_LE(land.forests.size(), Settings {}.forests.most);
	for (const auto& feature : land.features)
	{
		if (feature.forest != 0)
		{
			EXPECT_EQ(feature.kind, features::Kind::Tree);
			EXPECT_TRUE(ids.contains(feature.forest));
		}
	}
}
