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
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindDomains.h"
#include "ECS/ComputerMindRules.h"
#include "ECS/ComputerMindWorshipRules.h"

using namespace openblack;
using namespace openblack::computer_mind;
using namespace openblack::computer_mind_rules;

TEST(ComputerMindWorship, OwnsItsKindsOfNodes)
{
	EXPECT_EQ(DomainOf(NodeKind::ActionWorshipSiteThatNeedsVillagers), MindDomain::Worship);
	EXPECT_EQ(DomainOf(NodeKind::ObjectTakeVillagerFromTownToWorshipSite), MindDomain::Worship);
	EXPECT_EQ(DomainOf(NodeKind::ActionGiveBuildingSiteWorkers), MindDomain::Worship);
}

TEST(ComputerMindRules, WorshipNeeds)
{
	// A town with no one at worship needs worshippers fully, and one with most of its people there needs none
	EXPECT_FLOAT_EQ(TownWorshipNeed(10, 0, 0), 1.0f);
	EXPECT_FLOAT_EQ(TownWorshipNeed(0, 0, 0), 0.0f);
	EXPECT_FLOAT_EQ(TownWorshipNeed(10, 5, 1), 0.0f);
	// A third at worship leaves the cube of two thirds
	const float missing = 1.0f - 1.0f / 3.0f;
	EXPECT_FLOAT_EQ(TownWorshipNeed(3, 1, 0), (missing * missing) * missing);
	// More than all counts as all
	EXPECT_FLOAT_EQ(TownWorshipNeed(2, 3, 1), 0.0f);
	// A site with no icons needs no dancers; four dancers for each icon fill it
	EXPECT_FLOAT_EQ(SiteDancerNeed(0, 0), 0.0f);
	EXPECT_FLOAT_EQ(SiteDancerNeed(0, 3), 1.0f);
	EXPECT_FLOAT_EQ(SiteDancerNeed(12, 3), 0.0f);
	EXPECT_FLOAT_EQ(SiteDancerNeed(6, 3), 1.0f - 0.0625f);
}

TEST(ComputerMindWorshipRules, RestOfTheWorshippers)
{
	// The mean of what the dancers lack of life; a dancer that is not a villager counts with none
	const std::array<float, 4> lacks {0.5f, 0.25f, 0.0f, 0.25f};
	EXPECT_FLOAT_EQ(DancersRestDesire(lacks), 0.25f);
	EXPECT_FLOAT_EQ(DancersRestDesire({}), 0.0f);
	EXPECT_FLOAT_EQ(LifeDesire(0.75f), 0.25f);
	// A site under the least rest desire counts none; the urge is twice the most, at most all
	EXPECT_FLOAT_EQ(RestDesireCounted(0.19f), 0.0f);
	EXPECT_FLOAT_EQ(RestDesireCounted(0.2f), 0.2f);
	EXPECT_FLOAT_EQ(WorshippersRestUrge(0.3f), 0.6f);
	EXPECT_FLOAT_EQ(WorshippersRestUrge(0.5f), 1.0f);
	EXPECT_FLOAT_EQ(WorshippersRestUrge(0.9f), 1.0f);
	// A worshipper that lacks more than three quarters of its life is taken home with all the heart of the god
	EXPECT_FLOAT_EQ(TiredWorshipperUrge(0.75f), 0.75f);
	EXPECT_FLOAT_EQ(TiredWorshipperUrge(0.76f), 1.0f);
}

TEST(ComputerMindWorshipRules, WorksAndTheDesiresTheyServe)
{
	EXPECT_EQ(DesireServedBy(VillagerDisciple::Farmer), TownDesireInfo::ForFood);
	EXPECT_EQ(DesireServedBy(VillagerDisciple::Forester), TownDesireInfo::ForWood);
	EXPECT_EQ(DesireServedBy(VillagerDisciple::Fisherman), TownDesireInfo::ForFood);
	EXPECT_EQ(DesireServedBy(VillagerDisciple::Builder), TownDesireInfo::ToBuild);
	EXPECT_EQ(DesireServedBy(VillagerDisciple::Breeder), TownDesireInfo::ForChildren);
	EXPECT_FALSE(DesireServedBy(VillagerDisciple::None).has_value());
	EXPECT_FALSE(DesireServedBy(VillagerDisciple::Missionary).has_value());
	EXPECT_FALSE(DesireServedBy(VillagerDisciple::Worship).has_value());
}

TEST(ComputerMindWorshipRules, DiscipleThatATownWants)
{
	std::array<float, 13> desires {};
	desires.at(static_cast<size_t>(TownDesireInfo::ForWood)) = 1.0f;
	std::array<uint8_t, 13> disciples {};
	disciples.at(static_cast<size_t>(VillagerDisciple::Breeder)) = 2;
	// The town wants wood only: a forester
	const auto forester = MostWantedDisciple(disciples, 20, 0.0f, desires);
	ASSERT_TRUE(forester.has_value());
	EXPECT_EQ(forester->work, VillagerDisciple::Forester);
	EXPECT_GT(forester->value, 0.0f);
	// No adults free of worship: no disciple
	EXPECT_FALSE(MostWantedDisciple(disciples, 20, 1.0f, desires).has_value());
	EXPECT_FALSE(MostWantedDisciple(disciples, 0, 0.0f, desires).has_value());
	// With fewer than two breeders the town makes a breeder first, when it finds a villager for it
	disciples.at(static_cast<size_t>(VillagerDisciple::Breeder)) = 1;
	int asked = 0;
	const auto breeder = TownDiscipleWant(
	    disciples, 20, 0.0f, desires, [] { return 1.0f; },
	    [&asked] {
		    ++asked;
		    return 1.0f;
	    });
	ASSERT_TRUE(breeder.has_value());
	EXPECT_EQ(breeder->work, VillagerDisciple::Breeder);
	EXPECT_FLOAT_EQ(breeder->value, 1.0f);
	EXPECT_EQ(asked, 0);
	// With none found it wants the forester, as much as the free villager found is good
	const auto halfForester = TownDiscipleWant(disciples, 20, 0.0f, desires, [] { return 0.0f; }, [] { return 0.5f; });
	ASSERT_TRUE(halfForester.has_value());
	EXPECT_EQ(halfForester->work, VillagerDisciple::Forester);
	const auto whole = MostWantedDisciple(disciples, 20, 0.0f, desires);
	ASSERT_TRUE(whole.has_value());
	EXPECT_FLOAT_EQ(halfForester->value, 0.5f * whole->value);
}

TEST(ComputerMindWorshipRules, BuildingSites)
{
	// A site takes workers while its building takes builders and it wants more; a mending site only while it wants
	// mending
	EXPECT_TRUE(TakesWorkers({.maxBuilders = 4, .buildersNeeded = 2}));
	EXPECT_FALSE(TakesWorkers({.maxBuilders = 0, .buildersNeeded = 2}));
	EXPECT_FALSE(TakesWorkers({.maxBuilders = 4, .buildersNeeded = 0}));
	EXPECT_FALSE(TakesWorkers({.maxBuilders = 4, .buildersNeeded = 2, .built = true}));
	EXPECT_TRUE(TakesWorkers({.maxBuilders = 4, .buildersNeeded = 2, .built = true, .repairDesire = 0.5f}));
	EXPECT_TRUE(TakesWood({.woodNeeded = 10.0f}));
	EXPECT_FALSE(TakesWood({.woodNeeded = 0.0f}));
	EXPECT_FALSE(TakesWood({.built = true, .woodNeeded = 10.0f}));
	// Half the builders filled leaves half the want; all filled leaves none
	EXPECT_FLOAT_EQ(SiteWorkersValue(1.0f, 0.8f, 2, 4), 0.4f);
	EXPECT_FLOAT_EQ(SiteWorkersValue(1.0f, 0.8f, 9, 4), 0.0f);
	// Half the wood there leaves seven eighths of the want
	EXPECT_FLOAT_EQ(SiteWoodValue(0.5f, 50, 100.0f), 0.4375f);
	EXPECT_FLOAT_EQ(SiteWoodValue(1.0f, 200, 100.0f), 0.0f);
	EXPECT_FLOAT_EQ(FreeBuilderValue(0.5f, 0.5f, 0.5f), 0.125f);
	EXPECT_FLOAT_EQ(SiteWorkerDropRadius(8.0f), 10.0f);
	EXPECT_FLOAT_EQ(HomeDropDistance(8.0f, 1.0f), 11.0f);
}

TEST(ComputerMindWorshipRules, PlaceOfAWorkerForASite)
{
	const map_coords::MapCoords building {.x = 0x1000000, .z = 0x1000000};
	const map_coords::MapCoords start {};
	std::vector<float> angles;
	const auto offset = [&angles](float angle) {
		angles.push_back(angle);
		return map_coords::MapCoords {.x = static_cast<int32_t>(angles.size()), .z = 0};
	};
	const auto onMap = [](const map_coords::MapCoords&) { return true; };
	// With no wood about the god tries nine angles and keeps the last. The float sum of eight steps stays below a full
	// turn, so the ninth angle is almost the first.
	const auto last = SiteWorkerDropPlace(building, start, offset, onMap, [](const map_coords::MapCoords&) { return false; });
	EXPECT_EQ(angles.size(), 9u);
	EXPECT_EQ(last.x, building.x + 9);
	// It stops at the first point with wood
	angles.clear();
	const auto wood = SiteWorkerDropPlace(
	    building, start, offset, onMap, [&building](const map_coords::MapCoords& point) { return point.x == building.x + 3; });
	EXPECT_EQ(angles.size(), 3u);
	EXPECT_EQ(wood.x, building.x + 3);
	// Off the map it keeps the start
	angles.clear();
	const auto none = SiteWorkerDropPlace(
	    building, start, offset, [](const map_coords::MapCoords&) { return false; },
	    [](const map_coords::MapCoords&) { return false; });
	EXPECT_EQ(none, start);
}

TEST(ComputerMindWorship, OwnsTheMiraclesAtTheSites)
{
	EXPECT_EQ(DomainOf(NodeKind::ObjectCastFoodSpellOnWorshipSite), MindDomain::Worship);
	EXPECT_EQ(DomainOf(NodeKind::ObjectCastHealSpellAtWorshipSite), MindDomain::Worship);
	EXPECT_EQ(DomainOf(NodeKind::ObjectShallCastWoodSpellAgain), MindDomain::Worship);
}

TEST(ComputerMindWorshipRules, WorshipperWithTheLeastLife)
{
	// The first of the least wins
	const std::array<float, 4> lives {0.75f, 0.25f, 0.5f, 0.25f};
	EXPECT_EQ(LeastLife(lives), std::optional<size_t> {1});
	// Full life does not count, and no worshipper gives none
	const std::array<float, 2> full {1.0f, 1.0f};
	EXPECT_FALSE(LeastLife(full).has_value());
	EXPECT_FALSE(LeastLife({}).has_value());
	// A life just below full counts
	const std::array<float, 2> almost {1.0f, 0.99999994f};
	EXPECT_EQ(LeastLife(almost), std::optional<size_t> {1});
}
