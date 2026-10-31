/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The footpaths as a system, in a world of its own: a fake map that files what is made when it syncs, and a fake route
// planner that goes round what the map holds. A building that starts working gets a path from its town's pit, a town
// that joins a site finds its path over turns, a dance gets a path from the nearest town's pit, and a thing put on a path
// or taken off it sends the path round it or back when the changes are processed.

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/FootpathPlanning.h"
#include "ECS/FootpathWorld.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/FootpathSystem.h"
#include "ECS/WallHugRules.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace planning = openblack::ecs::footpath_planning;
using footpath_world::RouteStatus;

namespace
{

constexpr glm::vec3 k_PitPosition {100.0f, 0.0f, 100.0f};
constexpr glm::vec3 k_HousePosition {140.0f, 0.0f, 100.0f};
constexpr glm::vec3 k_RockPosition {120.0f, 0.0f, 100.0f};
constexpr float k_RockRadius = 3.0f;
/// How far past a thing's edge the fake planner goes round it, in metres
constexpr float k_DetourClearance = 1.0f;

struct Circle
{
	glm::vec2 centre;
	float radius;
};

/// Goes straight from the start to the goal, and round each circle across that line by a point beside it. As the game's
/// route search does, it gives the places between the ends, not the ends.
std::vector<glm::ivec2> PlanRound(glm::vec2 start, glm::vec2 goal, const std::vector<Circle>& circles)
{
	const auto line = goal - start;
	const float length = glm::length(line);
	if (length <= 0.0f)
	{
		return {};
	}
	const auto along = line / length;
	const glm::vec2 side {-along.y, along.x};
	std::vector<std::pair<float, glm::vec2>> detours;
	for (const auto& circle : circles)
	{
		const float t = glm::dot(circle.centre - start, along);
		const float off = glm::dot(circle.centre - start, side);
		if (t > 0.0f && t < length && std::abs(off) < circle.radius)
		{
			detours.emplace_back(t, circle.centre + side * (circle.radius + k_DetourClearance - off));
		}
	}
	std::ranges::sort(detours, {}, &std::pair<float, glm::vec2>::first);
	std::vector<glm::ivec2> places;
	std::ranges::transform(detours, std::back_inserter(places),
	                       [](const auto& detour) { return planning::ToPlace(detour.second); });
	return places;
}

/// A search that finds its way after a number of steps, round what the map holds when it starts, or finds none
class FakeRouteSearch final
{
public:
	FakeRouteSearch(int32_t stepsToFind, std::optional<std::vector<glm::ivec2>> route)
	    : _stepsLeft(stepsToFind)
	    , _route(std::move(route))
	{
	}

	void Step() { --_stepsLeft; }
	[[nodiscard]] RouteStatus GetStatus() const
	{
		if (_stepsLeft > 0)
		{
			return RouteStatus::Searching;
		}
		return _route.has_value() ? RouteStatus::Found : RouteStatus::Failed;
	}
	[[nodiscard]] std::optional<std::vector<glm::ivec2>> Route() const
	{
		return GetStatus() == RouteStatus::Found ? _route : std::nullopt;
	}

private:
	int32_t _stepsLeft;
	std::optional<std::vector<glm::ivec2>> _route;
};

/// The world of the tests: the things made wait for the map to sync, which files them and tells the footpaths
class FakeWorld final: public footpath_world::WorldInterface
{
public:
	[[nodiscard]] Registry& Entities() override { return registry; }
	[[nodiscard]] uint32_t Turn() const override { return turn; }
	void SyncMap() override
	{
		auto made = std::move(waiting);
		waiting.clear();
		for (const auto thing : made)
		{
			filed.insert(thing);
			if (system != nullptr)
			{
				system->ThingPut(thing);
			}
		}
	}
	[[nodiscard]] glm::ivec2 ArrivePlace(entt::entity thing) const override
	{
		if (const auto found = arrive.find(thing); found != arrive.end())
		{
			return found->second;
		}
		return wall_hug::ToWhole(glm::xz(registry.Get<const Transform>(thing).position));
	}
	[[nodiscard]] std::pair<glm::vec2, glm::vec2> PlannedEnds(entt::entity from, entt::entity to) const override
	{
		return {planning::ToPoint(ArrivePlace(from)), planning::ToPoint(ArrivePlace(to))};
	}
	[[nodiscard]] footpath_world::ObstacleLook ObstacleOf(entt::entity thing, glm::vec3 /*position*/) const override
	{
		footpath_world::ObstacleLook look {.known = true};
		if (const auto found = radii.find(thing); found != radii.end())
		{
			look.radius = found->second;
		}
		return look;
	}
	[[nodiscard]] footpath_world::Plan StartRoute(glm::vec2 start, glm::vec2 goal) const override
	{
		++searches;
		std::optional<std::vector<glm::ivec2>> route;
		if (findsWay)
		{
			std::vector<Circle> circles;
			for (const auto thing : filed)
			{
				const auto found = radii.find(thing);
				if (found != radii.end() && registry.Valid(thing))
				{
					circles.push_back({glm::xz(registry.Get<const Transform>(thing).position), found->second});
				}
			}
			route = PlanRound(start, goal, circles);
		}
		footpath_world::Plan plan;
		plan.start = start;
		plan.goal = goal;
		plan.live = true;
		fakeSearches.insert_or_assign(KeyOf(plan), FakeRouteSearch(stepsToFind, std::move(route)));
		return plan;
	}
	void MakeLive(footpath_world::Plan& plan) const override { plan.live = true; }
	void StepRoute(footpath_world::Plan& plan) const override { fakeSearches.at(KeyOf(plan)).Step(); }
	[[nodiscard]] RouteStatus StatusOfRoute(const footpath_world::Plan& plan) const override
	{
		return fakeSearches.at(KeyOf(plan)).GetStatus();
	}
	[[nodiscard]] std::optional<std::vector<glm::ivec2>> RouteOf(const footpath_world::Plan& plan) const override
	{
		return fakeSearches.at(KeyOf(plan)).Route();
	}
	[[nodiscard]] entt::entity TownOf(entt::entity building) const override
	{
		const auto found = towns.find(building);
		return found != towns.end() ? found->second : entt::null;
	}
	[[nodiscard]] std::optional<entt::entity> StoragePitOf(entt::entity town) const override
	{
		const auto found = pits.find(town);
		return found != pits.end() ? std::optional(found->second) : std::nullopt;
	}
	[[nodiscard]] bool IsBuilt(entt::entity building) const override { return !unbuilt.contains(building); }
	[[nodiscard]] float LandHeightAt(glm::vec2 /*point*/) const override { return 0.0f; }
	[[nodiscard]] float AltitudeAt(glm::ivec2 /*place*/) const override { return 0.0f; }
	void LookAgain(entt::entity walker) override { lookedAgain.push_back(walker); }
	void WalkTo(entt::entity walker, glm::ivec2 place) override { walkedTo.emplace_back(walker, place); }

	[[nodiscard]] static std::array<float, 4> KeyOf(const footpath_world::Plan& plan)
	{
		return {plan.start.x, plan.start.y, plan.goal.x, plan.goal.y};
	}

	/// A thing made at a position, waiting for the map; with a radius when the paths go round it
	entt::entity Make(glm::vec3 position, std::optional<float> radius = std::nullopt)
	{
		const auto thing = registry.Create();
		registry.Assign<Transform>(thing, position, glm::mat3(1.0f), glm::vec3(1.0f));
		if (radius.has_value())
		{
			radii[thing] = *radius;
		}
		waiting.push_back(thing);
		return thing;
	}

	/// The map takes a thing out, as it does when the thing goes or is picked up
	void TakeOut(entt::entity thing)
	{
		filed.erase(thing);
		system->ThingRemoved(thing, registry.Get<const Transform>(thing).position, true);
	}

	Registry registry;
	FootpathSystemInterface* system {nullptr};
	uint32_t turn {1};
	bool findsWay {true};
	int32_t stepsToFind {1};
	mutable int searches {0};
	/// The fake searches, found by the two ends of the plan that they belong to
	mutable std::map<std::array<float, 4>, FakeRouteSearch> fakeSearches;
	std::vector<entt::entity> waiting;
	std::set<entt::entity> filed;
	std::map<entt::entity, float> radii;
	std::map<entt::entity, glm::ivec2> arrive;
	std::map<entt::entity, entt::entity> towns;
	std::map<entt::entity, entt::entity> pits;
	std::set<entt::entity> unbuilt;
	std::vector<entt::entity> lookedAgain;
	std::vector<std::pair<entt::entity, glm::ivec2>> walkedTo;
};

class FootpathSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto made = std::make_unique<FakeWorld>();
		world = made.get();
		system = std::make_unique<FootpathSystem>(std::move(made));
		world->system = system.get();
		town = world->registry.Create();
		world->registry.Assign<Town>(town);
		world->registry.Assign<Transform>(town, k_PitPosition, glm::mat3(1.0f), glm::vec3(1.0f));
		pit = MakeAbode(k_PitPosition);
		world->pits[town] = pit;
		world->SyncMap();
	}

	entt::entity MakeAbode(glm::vec3 position)
	{
		const auto abode = world->Make(position);
		world->registry.Assign<Abode>(abode);
		world->towns[abode] = town;
		return abode;
	}

	/// The paths that a thing keeps, the newest first
	[[nodiscard]] std::vector<entt::entity> PathsOf(entt::entity thing) const
	{
		std::vector<entt::entity> paths;
		if (const auto* link = world->registry.TryGet<const FootpathLink>(thing))
		{
			std::ranges::transform(link->footpaths, std::back_inserter(paths),
			                       [](Footpath::Id id) { return static_cast<entt::entity>(id); });
		}
		return paths;
	}

	[[nodiscard]] std::vector<glm::ivec2> PlacesOf(entt::entity footpath) const
	{
		std::vector<glm::ivec2> places;
		std::ranges::transform(world->registry.Get<const Footpath>(footpath).nodes, std::back_inserter(places),
		                       &Footpath::Node::place);
		return places;
	}

	[[nodiscard]] size_t CountOfPaths() const
	{
		size_t count = 0;
		world->registry.Each<const Footpath>([&count](entt::entity, const Footpath&) { ++count; });
		return count;
	}

	[[nodiscard]] size_t CountOfFinders() const
	{
		size_t count = 0;
		world->registry.Each<const FootpathFinder>([&count](entt::entity, const FootpathFinder&) { ++count; });
		return count;
	}

	/// No node that walkers use stands inside the circle
	[[nodiscard]] bool WalkedNodesClearOf(entt::entity footpath, glm::vec3 centre, float radius) const
	{
		const auto& nodes = world->registry.Get<const Footpath>(footpath).nodes;
		return std::ranges::all_of(nodes, [centre, radius](const Footpath::Node& node) {
			return node.hidden || node.marker || glm::length(planning::ToPoint(node.place) - glm::xz(centre)) >= radius;
		});
	}

	FakeWorld* world {nullptr};
	std::unique_ptr<FootpathSystem> system;
	entt::entity town {entt::null};
	entt::entity pit {entt::null};
};

TEST_F(FootpathSystemTest, AWorkingBuildingGetsAPathFromThePitRoundWhatCameBeforeIt)
{
	const auto rock = world->Make(k_RockPosition, k_RockRadius);
	const auto house = MakeAbode(k_HousePosition);
	system->BuildingWorking(house);

	// The map synced before the path was planned, so the rock is filed and the path goes round it
	EXPECT_TRUE(world->filed.contains(rock));
	const auto paths = PathsOf(house);
	ASSERT_EQ(paths.size(), 1U);
	EXPECT_EQ(PathsOf(pit), paths);
	const auto places = PlacesOf(paths.front());
	ASSERT_EQ(places.size(), 3U);
	EXPECT_EQ(places.front(), world->ArrivePlace(pit));
	EXPECT_EQ(places.back(), world->ArrivePlace(house));
	EXPECT_TRUE(WalkedNodesClearOf(paths.front(), k_RockPosition, k_RockRadius));
	// The rock's change came before the path, so the path is not sent round it again: one search only
	EXPECT_EQ(world->searches, 1);
	EXPECT_TRUE(world->registry.Context().footpathObstacleChanges.empty());
	EXPECT_TRUE(std::ranges::none_of(world->registry.Get<const Footpath>(paths.front()).nodes,
	                                 [](const Footpath::Node& node) { return node.hidden || node.marker; }));
}

TEST_F(FootpathSystemTest, AMiracleDispenserOfATownGetsAPathFromThePitAsAnAbodeDoes)
{
	const auto dispenser = world->Make(k_HousePosition);
	world->registry.Assign<SpellDispenser>(dispenser);
	world->towns[dispenser] = town;
	system->BuildingWorking(dispenser);

	const auto paths = PathsOf(dispenser);
	ASSERT_EQ(paths.size(), 1U);
	EXPECT_EQ(PathsOf(pit), paths);
	const auto places = PlacesOf(paths.front());
	EXPECT_EQ(places.front(), world->ArrivePlace(pit));
	EXPECT_EQ(places.back(), world->ArrivePlace(dispenser));

	// A thing that is neither an abode nor a dispenser gets no path
	const auto rock = world->Make(k_RockPosition, k_RockRadius);
	world->towns[rock] = town;
	system->BuildingWorking(rock);
	EXPECT_TRUE(PathsOf(rock).empty());
}

TEST_F(FootpathSystemTest, NoPathAtTheFirstTurnForThePitItselfOrWithoutABuiltPit)
{
	const auto house = MakeAbode(k_HousePosition);
	world->turn = 0;
	system->BuildingWorking(house);
	EXPECT_TRUE(PathsOf(house).empty());

	world->turn = 1;
	system->BuildingWorking(pit);
	EXPECT_TRUE(PathsOf(pit).empty());

	world->unbuilt.insert(pit);
	system->BuildingWorking(house);
	EXPECT_TRUE(PathsOf(house).empty());

	world->unbuilt.clear();
	world->findsWay = false;
	system->BuildingWorking(house);
	EXPECT_TRUE(PathsOf(house).empty());
	EXPECT_EQ(CountOfPaths(), 0U);
}

TEST_F(FootpathSystemTest, AThingPutOnAPathSendsItRoundWhenTheChangesAreProcessed)
{
	const auto house = MakeAbode(k_HousePosition);
	system->BuildingWorking(house);
	const auto path = PathsOf(house).front();
	const auto straight = PlacesOf(path);
	ASSERT_EQ(straight.size(), 2U);

	world->Make(k_RockPosition, k_RockRadius);
	world->SyncMap();
	// The change waits for the next process, as the turn does it after the step that made the thing
	EXPECT_EQ(PlacesOf(path), straight);
	EXPECT_EQ(world->registry.Context().footpathObstacleChanges.size(), 1U);

	system->ProcessObstacleChanges();
	EXPECT_NE(PlacesOf(path), straight);
	EXPECT_TRUE(WalkedNodesClearOf(path, k_RockPosition, k_RockRadius));
	EXPECT_EQ(PlacesOf(path).front(), straight.front());
	EXPECT_EQ(PlacesOf(path).back(), straight.back());
	EXPECT_TRUE(world->registry.Context().footpathObstacleChanges.empty());
}

TEST_F(FootpathSystemTest, AThingTakenOffThePathBringsItBack)
{
	const auto house = MakeAbode(k_HousePosition);
	system->BuildingWorking(house);
	const auto path = PathsOf(house).front();
	const auto straight = PlacesOf(path);

	const auto rock = world->Make(k_RockPosition, k_RockRadius);
	world->SyncMap();
	system->ProcessObstacleChanges();
	ASSERT_NE(PlacesOf(path), straight);

	world->TakeOut(rock);
	system->ProcessObstacleChanges();
	EXPECT_EQ(PlacesOf(path), straight);
	EXPECT_TRUE(world->registry.Context().footpathObstacles.empty());
}

TEST_F(FootpathSystemTest, AThingThePathsGoThroughChangesNoPath)
{
	const auto house = MakeAbode(k_HousePosition);
	system->BuildingWorking(house);
	const auto path = PathsOf(house).front();
	const auto straight = PlacesOf(path);

	const auto bush = world->Make(k_RockPosition);
	world->SyncMap();
	EXPECT_TRUE(world->registry.Context().footpathObstacleChanges.empty());
	world->TakeOut(bush);
	EXPECT_TRUE(world->registry.Context().footpathObstacleChanges.empty());
	system->ProcessObstacleChanges();
	EXPECT_EQ(PlacesOf(path), straight);
}

TEST_F(FootpathSystemTest, AThingFarFromThePathLeavesItAsItIs)
{
	const auto house = MakeAbode(k_HousePosition);
	system->BuildingWorking(house);
	const auto path = PathsOf(house).front();
	const auto straight = PlacesOf(path);
	const int searchesBefore = world->searches;

	world->Make(k_RockPosition + glm::vec3(0.0f, 0.0f, 200.0f), k_RockRadius);
	world->SyncMap();
	system->ProcessObstacleChanges();
	EXPECT_EQ(PlacesOf(path), straight);
	EXPECT_EQ(world->searches, searchesBefore);
}

TEST_F(FootpathSystemTest, AWalkerOnAChangedStretchFindsItsWayOnTheNewNodes)
{
	const auto house = MakeAbode(k_HousePosition);
	system->BuildingWorking(house);
	const auto path = PathsOf(house).front();

	// A villager walks from the pit to the house, heading for the last node
	const auto villager = world->registry.Create();
	world->registry.Assign<LivingAction>(villager, VillagerStates::MoveOnPath, static_cast<uint16_t>(0));
	world->registry.Assign<FootpathWalk>(villager, path, false, std::optional<uint32_t>(1), entt::null);

	world->Make(k_RockPosition, k_RockRadius);
	world->SyncMap();
	system->ProcessObstacleChanges();

	const auto& walk = world->registry.Get<const FootpathWalk>(villager);
	const auto places = PlacesOf(path);
	ASSERT_TRUE(walk.node.has_value());
	ASSERT_LT(*walk.node, places.size());
	// It still walks to the house's end of the path, which is now further along the nodes
	EXPECT_EQ(places[*walk.node], places.back());
	EXPECT_GT(*walk.node, 1U);
}

TEST_F(FootpathSystemTest, ATownJoiningASiteFindsItsPathOverTurnsAndLaysItTheTurnAfter)
{
	const auto site = world->Make(k_HousePosition);
	world->SyncMap();
	world->stepsToFind = planning::k_StepsPerTurn * 2 + 5;
	system->TownJoinedSite(site, town);

	ASSERT_EQ(CountOfPaths(), 1U);
	EXPECT_TRUE(PathsOf(site).empty());
	EXPECT_TRUE(PathsOf(pit).empty());
	// Three turns to find the way, then it is laid on the fourth
	for (int i = 0; i < 3; ++i)
	{
		system->ProcessTurn();
		EXPECT_TRUE(PathsOf(site).empty()) << "turn " << i;
	}
	system->ProcessTurn();
	const auto paths = PathsOf(site);
	ASSERT_EQ(paths.size(), 1U);
	EXPECT_EQ(PathsOf(pit), paths);
	EXPECT_EQ(PlacesOf(paths.front()).front(), world->ArrivePlace(pit));
	EXPECT_EQ(PlacesOf(paths.front()).back(), world->ArrivePlace(site));
	EXPECT_TRUE(CountOfFinders() == 0);

	// A town that joins again finds the site's path already from its pit
	system->TownJoinedSite(site, town);
	EXPECT_TRUE(CountOfFinders() == 0);
	EXPECT_EQ(CountOfPaths(), 1U);
}

TEST_F(FootpathSystemTest, AFinderWhoseEndGoesOrThatFindsNoWayTakesItsPathAway)
{
	const auto site = world->Make(k_HousePosition);
	world->SyncMap();
	world->stepsToFind = planning::k_StepsPerTurn * 3;
	system->TownJoinedSite(site, town);
	system->ProcessTurn();
	world->registry.Destroy(site);
	system->ProcessTurn();
	EXPECT_EQ(CountOfPaths(), 0U);
	EXPECT_TRUE(CountOfFinders() == 0);

	const auto other = world->Make(k_HousePosition);
	world->SyncMap();
	world->findsWay = false;
	world->stepsToFind = 1;
	system->TownJoinedSite(other, town);
	// The turn that the search fails keeps the path; the next turn takes it away
	system->ProcessTurn();
	EXPECT_EQ(CountOfPaths(), 1U);
	system->ProcessTurn();
	EXPECT_EQ(CountOfPaths(), 0U);
	EXPECT_TRUE(PathsOf(other).empty());
}

TEST_F(FootpathSystemTest, OnlyADanceAwayFromSitesAndTemplesGetsAPathFromTheNearestTown)
{
	const auto makeDance = [this](DanceInfo type) {
		const auto dance = world->registry.Create();
		auto& data = world->registry.Assign<Dance>(dance);
		data.type = type;
		data.place = {
		    .x = map_coords::ToFixed(k_HousePosition.x), .z = map_coords::ToFixed(k_HousePosition.z), .altitude = 0.0f};
		world->arrive[dance] = wall_hug::ToWhole(glm::xz(k_HousePosition));
		return dance;
	};
	const auto worship = makeDance(DanceInfo::Worship);
	system->DanceMade(worship, town);
	const auto temple = makeDance(DanceInfo::CitadelDance_1);
	system->DanceMade(temple, town);
	EXPECT_EQ(CountOfPaths(), 0U);

	const auto dance = makeDance(DanceInfo::DanceTownCelebration);
	system->DanceMade(dance, entt::null);
	const auto paths = PathsOf(dance);
	ASSERT_EQ(paths.size(), 1U);
	EXPECT_EQ(PathsOf(pit), paths);
	// The path runs from the dance to the pit
	EXPECT_EQ(PlacesOf(paths.front()).front(), world->ArrivePlace(dance));
	EXPECT_EQ(PlacesOf(paths.front()).back(), world->ArrivePlace(pit));
	// A dance has no body, so it keeps its paths where they end
	const auto& link = world->registry.Get<const FootpathLink>(dance);
	EXPECT_FLOAT_EQ(link.position.x, planning::ToPoint(world->ArrivePlace(dance)).x);
}

} // namespace
