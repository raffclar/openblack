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
	    std::ranges::count_if(plan.town.buildings, [building](const auto& placed) { return placed.building == building; }));
}

size_t HousesOf(const Plan& plan)
{
	return static_cast<size_t>(std::ranges::count_if(plan.town.buildings, [](const auto& placed) {
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
	EXPECT_FLOAT_EQ(Distance(plan.town.x, plan.town.z, middleX, middleZ), settings.townDistance);
	EXPECT_FLOAT_EQ(Distance(plan.citadelX, plan.citadelZ, plan.town.x, plan.town.z),
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
		ASSERT_FALSE(plan.town.buildings.empty());
		// The town centre is first, at the place of the town
		EXPECT_EQ(plan.town.buildings.front().building, Building::TownCentre);
		EXPECT_EQ(plan.town.buildings.front().x, plan.town.x);
		EXPECT_EQ(plan.town.buildings.front().z, plan.town.z);
		EXPECT_EQ(CountOf(plan, Building::TownCentre), 1u);
		EXPECT_EQ(CountOf(plan, Building::StoragePit), 1u);
		EXPECT_EQ(CountOf(plan, Building::Creche), 1u);
		EXPECT_GE(HousesOf(plan), settings.fewestHouses);
		EXPECT_LE(HousesOf(plan), settings.mostHouses);
		EXPECT_TRUE(std::ranges::find(settlement::k_Tribes, plan.town.tribe) != settlement::k_Tribes.end());
	}
}

TEST(LandgenSettlement, TheSeedPicksEachTribe)
{
	const settlement::Settings settings;
	std::set<settlement::Tribe> tribes;
	for (uint64_t seed = 1; seed <= 200; ++seed)
	{
		tribes.insert(settlement::Make(k_StartX, k_StartZ, Clear(), seed, settings).town.tribe);
	}
	EXPECT_EQ(tribes.size(), settlement::k_Tribes.size());
}

TEST(LandgenSettlement, EachHouseHasItsAdults)
{
	const settlement::Settings settings;
	const auto plan = settlement::Make(k_StartX, k_StartZ, Clear(), 11, settings);
	const auto houses = HousesOf(plan);
	EXPECT_GE(plan.town.villagers.size(), houses * settings.fewestPeople);
	EXPECT_LE(plan.town.villagers.size(), houses * settings.mostPeople);
	const auto foresters = std::ranges::count_if(
	    plan.town.villagers, [](const auto& person) { return person.kind == settlement::Villager::Forester; });
	EXPECT_GT(foresters, 0);
	EXPECT_LT(static_cast<size_t>(foresters), plan.town.villagers.size());
	for (const auto& person : plan.town.villagers)
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
	for (size_t i = 0; i < plan.town.buildings.size(); ++i)
	{
		for (size_t j = i + 1; j < plan.town.buildings.size(); ++j)
		{
			EXPECT_GE(Distance(plan.town.buildings[i].x, plan.town.buildings[i].z, plan.town.buildings[j].x,
			                   plan.town.buildings[j].z),
			          nearest - 0.001f);
		}
		// No building is on the side of the temple
		EXPECT_GE(Distance(plan.town.buildings[i].x, plan.town.buildings[i].z, plan.citadelX, plan.citadelZ),
		          settings.citadelDistance + settings.townDistance - settings.buildingShift * 2.0f);
	}
}

TEST(LandgenSettlement, TheFieldsAreSquaresOfTheTenMetreLines)
{
	const settlement::Settings settings;
	const auto plan = settlement::Make(k_StartX, k_StartZ, Clear(), 5, settings);
	ASSERT_EQ(plan.town.fields.size(), static_cast<size_t>(settings.fieldRows * settings.fieldColumns));
	std::set<std::pair<float, float>> places;
	for (const auto& field : plan.town.fields)
	{
		EXPECT_FLOAT_EQ(std::fmod(field.x, 10.0f), 5.0f);
		EXPECT_FLOAT_EQ(std::fmod(field.z, 10.0f), 5.0f);
		places.insert({field.x, field.z});
		// The fields are past the houses, away from the temple
		EXPECT_GT(Distance(field.x, field.z, plan.citadelX, plan.citadelZ),
		          Distance(plan.town.x, plan.town.z, plan.citadelX, plan.citadelZ) + settings.buildingSpacing);
	}
	EXPECT_EQ(places.size(), plan.town.fields.size());
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
	for (const auto& placed : plan.town.buildings)
	{
		EXPECT_TRUE(free(placed.x, placed.z));
	}
	for (const auto& field : plan.town.fields)
	{
		EXPECT_TRUE(free(field.x, field.z));
	}
	EXPECT_LT(plan.town.fields.size(), static_cast<size_t>(settings.fieldRows * settings.fieldColumns));
}

TEST(LandgenSettlement, TheSameSeedGivesTheSamePlan)
{
	const settlement::Settings settings;
	const auto first = settlement::Make(k_StartX, k_StartZ, Clear(), 42, settings);
	const auto second = settlement::Make(k_StartX, k_StartZ, Clear(), 42, settings);
	ASSERT_EQ(first.town.buildings.size(), second.town.buildings.size());
	for (size_t i = 0; i < first.town.buildings.size(); ++i)
	{
		EXPECT_EQ(first.town.buildings[i].building, second.town.buildings[i].building);
		EXPECT_EQ(first.town.buildings[i].x, second.town.buildings[i].x);
		EXPECT_EQ(first.town.buildings[i].z, second.town.buildings[i].z);
		EXPECT_EQ(first.town.buildings[i].rotation, second.town.buildings[i].rotation);
	}
	EXPECT_EQ(first.town.villagers.size(), second.town.villagers.size());
	EXPECT_EQ(first.town.tribe, second.town.tribe);

	// Other seeds give other towns
	bool differs = false;
	for (uint64_t seed = 43; seed < 48 && !differs; ++seed)
	{
		const auto other = settlement::Make(k_StartX, k_StartZ, Clear(), seed, settings);
		differs = other.citadelX != first.citadelX || other.town.tribe != first.town.tribe ||
		          other.town.buildings.size() != first.town.buildings.size() ||
		          other.town.buildings[1].x != first.town.buildings[1].x;
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
	EXPECT_LT(Distance(land.settlement.town.x, land.settlement.town.z, middleX, middleZ), land.start.radius * 10.0f);
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

namespace
{
/// A flat fake land of 513 corners: 40 m high (60 altitudes), 150 m from the sea everywhere and far from rivers
struct FakeLand
{
	Grid<float> heights {513, 60.0f};
	Grid<float> sea {513, 15.0f};
	Grid<float> rivers {513, 100.0f};
};

float MinimumDistance(const std::vector<settlement::Site>& sites, settlement::Site first, size_t index)
{
	float closest = Distance(sites[index].x, sites[index].z, first.x, first.z);
	for (size_t j = 0; j < sites.size(); ++j)
	{
		if (j != index)
		{
			closest = std::min(closest, Distance(sites[index].x, sites[index].z, sites[j].x, sites[j].z));
		}
	}
	return closest;
}
} // namespace

TEST(LandgenTowns, TheLandHasAsManyTownsAsALandOfTheStory)
{
	const FakeLand land;
	const settlement::SiteSettings settings;
	const settlement::Site first {.x = 256.0f, .z = 256.0f};
	std::set<size_t> counts;
	for (uint64_t seed = 1; seed <= 40; ++seed)
	{
		const auto sites =
		    settlement::FindSites(land.heights, HeightCurve {}, land.sea, land.rivers, first, {}, seed, settings);
		// The player's town and the neutral towns. A count of the story above the most neutral towns stops at it.
		const auto towns = sites.size() + 1;
		const bool ofTheStory =
		    std::ranges::find(settings.townCounts, static_cast<uint32_t>(towns)) != settings.townCounts.end();
		EXPECT_TRUE(ofTheStory || towns == settings.mostNeutral + 1) << towns;
		EXPECT_LE(sites.size(), settings.mostNeutral);
		counts.insert(towns);
	}
	EXPECT_EQ(counts, (std::set<size_t> {6, 8, 10, 11}));
}

TEST(LandgenTowns, EachTownIsInTheReachOfATownBeforeItAndNotTooNear)
{
	const FakeLand land;
	const settlement::SiteSettings settings;
	const settlement::Site first {.x = 256.0f, .z = 256.0f};
	const auto sites = settlement::FindSites(land.heights, HeightCurve {}, land.sea, land.rivers, first, {}, 5, settings);
	ASSERT_FALSE(sites.empty());
	for (size_t i = 0; i < sites.size(); ++i)
	{
		EXPECT_GE(MinimumDistance(sites, first, i) * 10.0f, settings.nearestTown - 0.01f);
		// The nearest town before this town is in the reach
		float before = Distance(sites[i].x, sites[i].z, first.x, first.z);
		for (size_t j = 0; j < i; ++j)
		{
			before = std::min(before, Distance(sites[i].x, sites[i].z, sites[j].x, sites[j].z));
		}
		EXPECT_LE(before * 10.0f, settings.farthestTown + 0.01f);
	}
}

TEST(LandgenTowns, NoTownStandsOnGroundOutOfTheRules)
{
	FakeLand land;
	const settlement::SiteSettings settings;
	// The west half of the map is a set of bands that each break one rule. The east half is good ground.
	for (int x = 0; x < 256; ++x)
	{
		for (int z = 0; z < 513; ++z)
		{
			switch ((x / 32) % 5)
			{
			case 0:
				// 3.4 m: too low
				land.heights.At(x, z) = 5.0f;
				break;
			case 1:
				// 154 m: too high
				land.heights.At(x, z) = 230.0f;
				break;
			case 2:
				// 40 m from the sea
				land.sea.At(x, z) = 4.0f;
				break;
			case 3:
				// 400 m from the sea
				land.sea.At(x, z) = 40.0f;
				break;
			default:
				// On the bank of a river
				land.rivers.At(x, z) = 1.0f;
				break;
			}
		}
	}
	// A rough strip: the ground rises 40 m at each second corner
	for (int x = 256; x < 300; ++x)
	{
		for (int z = 0; z < 513; ++z)
		{
			land.heights.At(x, z) = 60.0f + (((x + z) % 2 == 0) ? 60.0f : 0.0f);
		}
	}
	const settlement::Site first {.x = 400.0f, .z = 256.0f};
	for (uint64_t seed = 1; seed <= 10; ++seed)
	{
		const auto sites =
		    settlement::FindSites(land.heights, HeightCurve {}, land.sea, land.rivers, first, {}, seed, settings);
		for (const auto& site : sites)
		{
			EXPECT_GE(site.x, 300.0f);
		}
	}
}

TEST(LandgenTowns, NoTownMakesTheGroundOfTheStartFlat)
{
	const FakeLand land;
	const settlement::SiteSettings settings;
	const settlement::Site first {.x = 256.0f, .z = 256.0f};
	const std::array keepOut {settlement::Area {.x = 251.0f, .z = 256.0f, .radius = 15.0f}};
	for (uint64_t seed = 1; seed <= 10; ++seed)
	{
		const auto sites =
		    settlement::FindSites(land.heights, HeightCurve {}, land.sea, land.rivers, first, keepOut, seed, settings);
		for (const auto& site : sites)
		{
			EXPECT_GE(Distance(site.x, site.z, keepOut[0].x, keepOut[0].z), keepOut[0].radius + settings.blendRadius);
		}
	}
}

TEST(LandgenTowns, TheSameSeedGivesTheSamePlaces)
{
	const FakeLand land;
	const settlement::SiteSettings settings;
	const settlement::Site first {.x = 256.0f, .z = 256.0f};
	const auto one = settlement::FindSites(land.heights, HeightCurve {}, land.sea, land.rivers, first, {}, 9, settings);
	const auto two = settlement::FindSites(land.heights, HeightCurve {}, land.sea, land.rivers, first, {}, 9, settings);
	ASSERT_EQ(one.size(), two.size());
	for (size_t i = 0; i < one.size(); ++i)
	{
		EXPECT_EQ(one[i].x, two[i].x);
		EXPECT_EQ(one[i].z, two[i].z);
	}
}

TEST(LandgenTowns, TheLandHasFourToSixTribesWithThePlayersTribeFirst)
{
	const settlement::SiteSettings settings;
	std::set<size_t> counts;
	for (uint64_t seed = 1; seed <= 40; ++seed)
	{
		const auto tribes = settlement::TribesOf(settlement::Tribe::Greek, seed, settings);
		ASSERT_FALSE(tribes.empty());
		EXPECT_EQ(tribes.front(), settlement::Tribe::Greek);
		EXPECT_EQ(std::set<settlement::Tribe>(tribes.begin(), tribes.end()).size(), tribes.size());
		EXPECT_GE(tribes.size(), 4u);
		EXPECT_LE(tribes.size(), 6u);
		counts.insert(tribes.size());
	}
	EXPECT_EQ(counts.size(), 3u);
}

TEST(LandgenTowns, EachOtherTribeOfTheLandHasATown)
{
	const std::vector tribes {settlement::Tribe::Norse, settlement::Tribe::Celtic, settlement::Tribe::Japanese,
	                          settlement::Tribe::Indian};
	const auto towns = settlement::TribesOfTowns(tribes, 9, 3);
	ASSERT_EQ(towns.size(), 9u);
	for (size_t i = 1; i < tribes.size(); ++i)
	{
		EXPECT_TRUE(std::ranges::find(towns, tribes[i]) != towns.end());
	}
	for (const auto tribe : towns)
	{
		EXPECT_TRUE(std::ranges::find(tribes, tribe) != tribes.end());
	}
	// A land with fewer towns than tribes gives each town a tribe
	EXPECT_EQ(settlement::TribesOfTowns(tribes, 2, 3).size(), 2u);
}

TEST(LandgenTowns, ANeutralTownHasTheBuildingsOfATownOfTheStart)
{
	const settlement::Settings settings;
	size_t workshops = 0;
	size_t graveyards = 0;
	constexpr uint32_t k_Towns = 200;
	for (uint32_t number = 0; number < k_Towns; ++number)
	{
		const auto town =
		    settlement::MakeTown({.x = k_StartX, .z = k_StartZ}, settlement::Tribe::Aztec, Clear(), 7, number, settings);
		EXPECT_EQ(town.tribe, settlement::Tribe::Aztec);
		ASSERT_FALSE(town.buildings.empty());
		EXPECT_EQ(town.buildings.front().building, Building::TownCentre);
		EXPECT_FLOAT_EQ(town.x, k_StartX * 10.0f);
		EXPECT_FLOAT_EQ(town.z, k_StartZ * 10.0f);
		Plan plan;
		plan.town = town;
		EXPECT_EQ(CountOf(plan, Building::StoragePit), 1u);
		EXPECT_EQ(CountOf(plan, Building::Creche), 1u);
		EXPECT_GE(HousesOf(plan), settings.fewestHouses);
		EXPECT_LE(HousesOf(plan), settings.mostHouses);
		EXPECT_EQ(town.fields.size(), static_cast<size_t>(settings.fieldRows * settings.fieldColumns));
		EXPECT_GE(town.villagers.size(), HousesOf(plan) * settings.fewestPeople);
		workshops += CountOf(plan, Building::Workshop);
		graveyards += CountOf(plan, Building::Graveyard);
		// The buildings do not touch
		const float nearest = settings.buildingSpacing - (2.0f * settings.buildingShift);
		for (size_t i = 0; i < town.buildings.size(); ++i)
		{
			for (size_t j = i + 1; j < town.buildings.size(); ++j)
			{
				EXPECT_GE(Distance(town.buildings[i].x, town.buildings[i].z, town.buildings[j].x, town.buildings[j].z),
				          nearest - 0.001f);
			}
		}
	}
	// Near the shares of the towns of the story: 35 of 59 with a workshop and 31 of 59 with a graveyard
	EXPECT_NEAR(static_cast<float>(workshops) / k_Towns, settings.workshopShare, 0.1f);
	EXPECT_NEAR(static_cast<float>(graveyards) / k_Towns, settings.graveyardShare, 0.1f);
}

TEST(LandgenTowns, TheTownOfTheStartIsWhereThePlanPutsIt)
{
	const settlement::Settings settings;
	for (uint64_t seed = 1; seed <= 10; ++seed)
	{
		const auto plan = settlement::Make(k_StartX, k_StartZ, Clear(), seed, settings);
		const auto site = settlement::TownOfStart(k_StartX, k_StartZ, seed, settings);
		EXPECT_FLOAT_EQ(site.x * 10.0f, plan.town.x);
		EXPECT_FLOAT_EQ(site.z * 10.0f, plan.town.z);
	}
}

TEST(LandgenTowns, TheTownsOfAGeneratedLandKeepTheirRules)
{
	const auto land = Generate(SmallSettings(77));
	const settlement::SiteSettings rules;
	for (const auto& town : land.settlement.neutral)
	{
		EXPECT_GE(Distance(town.x, town.z, land.settlement.town.x, land.settlement.town.z), rules.nearestTown - 0.01f);
		// The ground of the town is flat
		const auto cx = static_cast<int>(town.x / 10.0f);
		const auto cz = static_cast<int>(town.z / 10.0f);
		const auto centre = land.cells.At(cx, cz).altitude;
		for (int d = -4; d <= 4; ++d)
		{
			EXPECT_NEAR(land.cells.At(cx + d, cz).altitude, centre, 1);
			EXPECT_NEAR(land.cells.At(cx, cz + d).altitude, centre, 1);
		}
	}
	// No tree grows on the ground of a town
	for (const auto& feature : land.features)
	{
		for (const auto& town : land.settlement.neutral)
		{
			EXPECT_GE(Distance(feature.x, feature.z, town.x, town.z), rules.blendRadius * 10.0f - 0.01f);
		}
	}
}

TEST(LandgenTowns, TheHeightsOfTheTownsSpreadAsTheTownsOfTheStory)
{
	// Flat squares of 40 corners at heights from 6 m to 146 m in steps of 10 m
	FakeLand land;
	for (int x = 0; x < 513; ++x)
	{
		for (int z = 0; z < 513; ++z)
		{
			const int step = (((x / 40) * 7) + ((z / 40) * 3)) % 15;
			land.heights.At(x, z) = (6.0f + (10.0f * static_cast<float>(step))) / 0.67f;
		}
	}
	const settlement::SiteSettings settings;
	const settlement::Site first {.x = 256.0f, .z = 256.0f};
	std::vector<float> heights;
	for (uint64_t seed = 1; seed <= 30; ++seed)
	{
		for (const auto& site :
		     settlement::FindSites(land.heights, HeightCurve {}, land.sea, land.rivers, first, {}, seed, settings))
		{
			heights.push_back(land.heights.At(static_cast<int>(site.x), static_cast<int>(site.z)) * 0.67f);
		}
	}
	ASSERT_GT(heights.size(), 50u);
	std::ranges::sort(heights);
	// The median of the towns of each land of the story is from 27.5 m to 50.2 m
	const float median = heights[heights.size() / 2];
	EXPECT_GE(median, 27.5f);
	EXPECT_LE(median, 50.2f);
	EXPECT_LT(heights.front(), 27.5f);
	EXPECT_GT(heights.back(), 75.0f);
}

TEST(LandgenTowns, AStartFarFromTheGoodGroundStillHasTowns)
{
	// Only the ground east of corner 300 is near enough to the sea. The start is 250 corners away from it.
	FakeLand land;
	for (int x = 0; x < 300; ++x)
	{
		for (int z = 0; z < 513; ++z)
		{
			land.sea.At(x, z) = 60.0f;
		}
	}
	const settlement::SiteSettings settings;
	const settlement::Site first {.x = 50.0f, .z = 256.0f};
	const auto sites = settlement::FindSites(land.heights, HeightCurve {}, land.sea, land.rivers, first, {}, 3, settings);
	ASSERT_GE(sites.size(), 5u);
	// The first town is on the nearest good ground, and the chain goes on from it
	EXPECT_LT(sites.front().x, 310.0f);
	for (const auto& site : sites)
	{
		EXPECT_GE(site.x, 300.0f);
	}
}
