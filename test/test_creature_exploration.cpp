/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <set>
#include <utility>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureDance.h"
#include "Creature/CreatureExplorationAgendas.h"
#include "Creature/CreatureExplorationMap.h"
#include "Creature/CreaturePlanActions.h"

using namespace openblack;
using namespace openblack::creature_exploration;
using creature_mind::Effect;
using creature_mind::Movement;
using creature_mind::ObjectOrder;
using creature_mind::Step;
namespace plan_actions = openblack::creature_plan_actions;

namespace
{
/// A land with blocks only west of region 20: flat at 10 metres, with region (5, 5) at 30 metres, and coast cells along
/// its east edge. East of it there is open sea.
std::optional<CellLand> FakeCell(glm::ivec2 cell)
{
	constexpr int32_t k_LandCells = 20 * k_CellsPerRegion;
	if (cell.x >= k_LandCells)
	{
		return std::nullopt;
	}
	const bool high = cell.x / k_CellsPerRegion == 5 && cell.y / k_CellsPerRegion == 5;
	return CellLand {.height = high ? 30.0f : 10.0f, .water = false, .coast = cell.x == k_LandCells - 1};
}

/// The cells of towns
struct FakeTowns
{
	std::set<std::pair<int32_t, int32_t>> cells;

	[[nodiscard]] TownAt At() const
	{
		return [this](glm::ivec2 cell) { return cells.contains({cell.x, cell.y}); };
	}
};

uint32_t First(uint32_t /*range*/)
{
	return 0;
}

uint32_t Second(uint32_t range)
{
	return range > 1 ? 1 : 0;
}
} // namespace

TEST(CreatureExploration, RegionsTakeTheirKindsFromTheirCells)
{
	const FakeTowns towns {.cells = {{42, 43}}};
	const auto map = RegionMap::Make(FakeCell, towns.At());
	EXPECT_FLOAT_EQ(map.Highest(), 30.0f);
	// Only the high region is a hill: the flat land is not higher than half the highest region
	EXPECT_TRUE(map.At({5, 5}).Is(Kind::Hill));
	EXPECT_FALSE(map.At({4, 5}).Is(Kind::Hill));
	EXPECT_TRUE(map.At({5, 5}).Is(Kind::Town));
	EXPECT_FALSE(map.At({6, 5}).Is(Kind::Town));
	EXPECT_TRUE(map.At({19, 3}).Is(Kind::Coast));
	EXPECT_TRUE(map.At({19, 3}).Is(Kind::Land));
	EXPECT_FALSE(map.At({19, 3}).Is(Kind::Sea));
	// Cells off the land blocks are sea, with no height
	EXPECT_TRUE(map.At({20, 3}).Is(Kind::Sea));
	EXPECT_FALSE(map.At({20, 3}).Is(Kind::Land));
	EXPECT_FLOAT_EQ(map.At({20, 3}).highest, 0.0f);
}

TEST(CreatureExploration, TheLastOfTheHighestCellsCounts)
{
	const auto map = RegionMap::Make(FakeCell, FakeTowns {}.At());
	EXPECT_EQ(map.At({0, 0}).highestCell, glm::ivec2(7, 7));
}

TEST(CreatureExploration, TheCellOfARegionDependsOnTheKind)
{
	FakeTowns towns {.cells = {{42, 43}, {44, 41}}};
	const auto map = RegionMap::Make(FakeCell, towns.At());
	// The first town cell in the order x then z
	EXPECT_EQ(CellFor(map, {5, 5}, Kind::Town, towns.At()), glm::ivec2(42, 43));
	// The town buildings are gone: the first cell of the region
	towns.cells.clear();
	EXPECT_EQ(CellFor(map, {5, 5}, Kind::Town, towns.At()), glm::ivec2(40, 40));
	EXPECT_EQ(CellFor(map, {19, 3}, Kind::Coast, towns.At()), glm::ivec2(159, 24));
	EXPECT_EQ(CellFor(map, {20, 3}, Kind::Sea, towns.At()), glm::ivec2(160, 24));
	EXPECT_EQ(CellFor(map, {5, 5}, Kind::Hill, towns.At()), glm::ivec2(47, 47));
	EXPECT_EQ(CellFor(map, {2, 3}, Kind::Land, towns.At()), glm::ivec2(20, 28));
	EXPECT_EQ(PlaceOf({20, 28}), glm::vec2(200.0f, 280.0f));
}

TEST(CreatureExploration, TheSearchGoesOutInASpiralAndCanPreferNewRegions)
{
	// Towns west and east of the start, and one far away
	const FakeTowns towns {.cells = {{72, 80}, {88, 80}, {240, 240}}};
	const auto map = RegionMap::Make([](glm::ivec2) { return CellLand {.height = 10.0f}; }, towns.At());
	Visited visited;
	const Search search {.kind = Kind::Town, .from = {10, 10}, .preferUnvisited = true};
	// The spiral goes west first
	EXPECT_EQ(FindNearest(map, visited, search), glm::ivec2(9, 10));
	Visit(visited, {9, 10});
	EXPECT_EQ(FindNearest(map, visited, search), glm::ivec2(11, 10));
	Visit(visited, {11, 10});
	Visit(visited, {30, 30});
	// When it went into all of them, the search gives the last region that it found
	EXPECT_EQ(FindNearest(map, visited, search), glm::ivec2(30, 30));
	// Without the preference, the nearest
	EXPECT_EQ(FindNearest(map, visited, {.kind = Kind::Town, .from = {10, 10}}), glm::ivec2(9, 10));
	// A flat land with no water has no coast
	EXPECT_FALSE(FindNearest(map, visited, {.kind = Kind::Coast, .from = {10, 10}}).has_value());
}

TEST(CreatureExploration, TheStartCanBeLeftOut)
{
	const FakeTowns towns {.cells = {{80, 80}, {96, 80}}};
	const auto map = RegionMap::Make([](glm::ivec2) { return CellLand {.height = 10.0f}; }, towns.At());
	const Visited visited;
	EXPECT_EQ(FindNearest(map, visited, {.kind = Kind::Town, .from = {10, 10}}), glm::ivec2(10, 10));
	EXPECT_EQ(FindNearest(map, visited, {.kind = Kind::Town, .from = {10, 10}, .includeStart = false}), glm::ivec2(12, 10));
}

TEST(CreatureExploration, ACreatureNotesTheRegionsItGoesInto)
{
	Visited visited;
	EXPECT_EQ(RegionOf({85.0f, 160.0f}), glm::ivec2(1, 2));
	Visit(visited, RegionOf({85.0f, 160.0f}));
	EXPECT_TRUE(WasVisited(visited, {1, 2}));
	EXPECT_FALSE(WasVisited(visited, {2, 1}));
	// Off the map, nothing changes
	Visit(visited, {-1, 0});
	EXPECT_EQ(visited.count(), 1);
}

TEST(CreatureExploration, AYoungOrRestedCreatureWantsToExplore)
{
	EXPECT_TRUE(WanderlustAllowed(5, 1.0f));
	EXPECT_TRUE(WanderlustAllowed(6, 0.79f));
	EXPECT_FALSE(WanderlustAllowed(6, 0.8f));
}

TEST(CreatureExploration, TheRidgeIsACircleRoundTheHill)
{
	const glm::vec2 hill {100.0f, 200.0f};
	const auto points = RidgePoints(hill);
	EXPECT_NEAR(points[0].x, 115.0f, 0.01f);
	EXPECT_NEAR(points[0].y, 200.0f, 0.01f);
	for (const auto point : points)
	{
		EXPECT_NEAR(glm::distance(point, hill), k_RidgeRadius, 0.01f);
	}
}

TEST(CreatureExploration, GoingAlongARidgeIsCompleteAtTheHill)
{
	const auto agenda = GoToHillAndWalkAlongRidge({100.0f, 200.0f}, 10.0f);
	ASSERT_EQ(agenda.size(), 1 + k_RidgePoints);
	EXPECT_EQ(agenda[0].effect, Effect::Completed);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 30.0f);
	EXPECT_FLOAT_EQ(agenda[1].movement.maxDistance, 20.0f);
	EXPECT_EQ(GoToHillAndLook({0.0f, 0.0f}, 10.0f)[0].movement.maxDistance, 20.0f);
	ASSERT_EQ(GoToHillAndSit({0.0f, 0.0f}, 10.0f).size(), 1);
}

TEST(CreatureExploration, LookingAtTheMountainsPointsOneTimeInThree)
{
	const auto points = LookAtMountains({50.0f, 60.0f}, First);
	ASSERT_EQ(points.size(), 3);
	EXPECT_EQ(points[1].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(points[1].effect, Effect::Completed);
	EXPECT_EQ(points[2].movement.kind, Movement::Kind::TurnToFace);
	EXPECT_EQ(LookAtMountains({50.0f, 60.0f}, Second).size(), 2);
}

TEST(CreatureExploration, LookingOutToSeaGoesToTheCoast)
{
	const auto agenda = LookOutToSea({10.0f, 0.0f}, {0.0f, 0.0f}, 5.0f, First);
	ASSERT_EQ(agenda.size(), 3);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(10.0f, 0.0f));
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 10.0f);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFace);
	EXPECT_EQ(LookOutToSea({10.0f, 0.0f}, {0.0f, 0.0f}, 5.0f, Second).size(), 2);
}

TEST(CreatureExploration, SittingOnTheBeachFacesTheSea)
{
	const auto agenda = SitDownOnBeach({10.0f, 0.0f}, {0.0f, 0.0f}, 5.0f, First);
	ASSERT_EQ(agenda.size(), 3);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 5.0f);
	EXPECT_EQ(agenda[1].movement.point, glm::vec2(0.0f, 0.0f));
	EXPECT_EQ(agenda[2].kind, Step::Kind::Static);
	EXPECT_GE(agenda[2].seconds, k_BeachSitSeconds);
	EXPECT_LE(agenda[2].seconds, k_BeachSitSeconds + k_BeachSitExtraSeconds);
}

TEST(CreatureExploration, ThrowingIntoTheSeaPicksUpOnlyWithAnEmptyHand)
{
	const auto agenda = ThrowInTheSea(7u, {10.0f, 0.0f}, {0.0f, 0.0f}, 5.0f);
	ASSERT_EQ(agenda.size(), 3);
	EXPECT_EQ(agenda[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[2].order.kind, ObjectOrder::Kind::Throw);
	EXPECT_EQ(agenda[2].order.point, glm::vec2(0.0f, 0.0f));
	EXPECT_EQ(ThrowInTheSea(std::nullopt, {10.0f, 0.0f}, {0.0f, 0.0f}, 5.0f).size(), 2);
}

TEST(CreatureExploration, TheActionsNeedTheirPlaces)
{
	const auto* mountains = plan_actions::For("LookAtMountains");
	ASSERT_NE(mountains, nullptr);
	plan_actions::Situation situation;
	EXPECT_FALSE(plan_actions::Agenda(*mountains, std::nullopt, {}, situation, First).has_value());
	situation.explorePlace = glm::vec2 {50.0f, 50.0f};
	EXPECT_TRUE(plan_actions::Agenda(*mountains, std::nullopt, {}, situation, First).has_value());
	// Looking out to sea needs the sea as well
	const auto* sea = plan_actions::For("LookOutToSea");
	ASSERT_NE(sea, nullptr);
	EXPECT_FALSE(plan_actions::Agenda(*sea, std::nullopt, {}, situation, First).has_value());
	situation.exploreSea = glm::vec2 {0.0f, 0.0f};
	EXPECT_TRUE(plan_actions::Agenda(*sea, std::nullopt, {}, situation, First).has_value());
	// Holding something, it throws that into the sea with no pick-up first
	const auto* thrown = plan_actions::For("ThrowInTheSea");
	ASSERT_NE(thrown, nullptr);
	situation.handFull = true;
	EXPECT_EQ(plan_actions::Agenda(*thrown, 7u, {}, situation, First)->size(), 2);
	for (const auto* action : {"GoToHillAndLook", "GoToHillAndSit", "GoToHillAndWalkAlongRidge", "ExploreCoastline",
	                           "ExploreTowns", "SitDownOnBeach"})
	{
		EXPECT_NE(plan_actions::For(action), nullptr) << action;
	}
	EXPECT_FALSE(plan_actions::Possible(*plan_actions::For("SitDownOnBeach"), {.stage = 2}));
}

TEST(CreatureExploration, DancingByTheSeaGoesToTheCoastAndDancesForTwentySeconds)
{
	const auto agenda = DanceByTheSea({10.0f, 0.0f}, {0.0f, 0.0f}, 5.0f);
	ASSERT_EQ(agenda.size(), 3);
	// It walks to within one of its heights of the coast, with a smile
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(10.0f, 0.0f));
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 5.0f);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Smile);
	// It turns to the sea
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFace);
	EXPECT_EQ(agenda[1].movement.point, glm::vec2(0.0f, 0.0f));
	// It dances alone with a playful face
	EXPECT_EQ(agenda[2].kind, Step::Kind::PracticeDance);
	EXPECT_FLOAT_EQ(agenda[2].seconds, creature_dance::k_PracticeSeconds);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Playfulness);
	EXPECT_EQ(agenda[2].effect, Effect::None);
}

TEST(CreatureExploration, DancingByTheSeaNeedsTheCoastAndTheSea)
{
	const auto* dance = plan_actions::For("DanceOnYourOwnByTheSea");
	ASSERT_NE(dance, nullptr);
	EXPECT_FALSE(dance->leavesHome);
	plan_actions::Situation situation;
	EXPECT_FALSE(plan_actions::Agenda(*dance, std::nullopt, {}, situation, First).has_value());
	situation.explorePlace = glm::vec2 {10.0f, 0.0f};
	EXPECT_FALSE(plan_actions::Agenda(*dance, std::nullopt, {}, situation, First).has_value());
	situation.exploreSea = glm::vec2 {0.0f, 0.0f};
	EXPECT_EQ(plan_actions::Agenda(*dance, std::nullopt, {}, situation, First)->size(), 3);
}
