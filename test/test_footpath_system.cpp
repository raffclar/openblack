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

#define LOCATOR_IMPLEMENTATIONS 1

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
	[[nodiscard]] float ArriveAltitude(entt::entity thing) const override
	{
		const auto found = arriveAltitudes.find(thing);
		return found != arriveAltitudes.end() ? found->second : 0.0f;
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
			look.altitude = ArriveAltitude(thing);
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
	void WalkTo(entt::entity walker, glm::ivec2 place, float altitude) override
	{
		walkedTo.emplace_back(walker, place);
		walkedToAltitudes.push_back(altitude);
	}

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
	/// The heights above the land where the paths to things end, and of the places of things that the paths go round
	std::map<entt::entity, float> arriveAltitudes;
	std::map<entt::entity, entt::entity> towns;
	std::map<entt::entity, entt::entity> pits;
	std::set<entt::entity> unbuilt;
	std::vector<entt::entity> lookedAgain;
	std::vector<std::pair<entt::entity, glm::ivec2>> walkedTo;
	std::vector<float> walkedToAltitudes;
};

class FootpathSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto made = std::make_unique<FakeWorld>();
		_world = made.get();
		_system = std::make_unique<FootpathSystem>(std::move(made));
		_world->system = _system.get();
		_town = _world->registry.Create();
		_world->registry.Assign<Town>(_town);
		_world->registry.Assign<Transform>(_town, k_PitPosition, glm::mat3(1.0f), glm::vec3(1.0f));
		_pit = MakeAbode(k_PitPosition);
		_world->pits[_town] = _pit;
		_world->SyncMap();
	}

	entt::entity MakeAbode(glm::vec3 position)
	{
		const auto abode = _world->Make(position);
		_world->registry.Assign<Abode>(abode);
		_world->towns[abode] = _town;
		return abode;
	}

	/// The paths that a thing keeps, the newest first
	[[nodiscard]] std::vector<entt::entity> PathsOf(entt::entity thing) const
	{
		std::vector<entt::entity> paths;
		if (const auto* link = _world->registry.TryGet<const FootpathLink>(thing))
		{
			std::ranges::transform(link->footpaths, std::back_inserter(paths),
			                       [](Footpath::Id id) { return static_cast<entt::entity>(id); });
		}
		return paths;
	}

	[[nodiscard]] std::vector<glm::ivec2> PlacesOf(entt::entity footpath) const
	{
		std::vector<glm::ivec2> places;
		std::ranges::transform(_world->registry.Get<const Footpath>(footpath).nodes, std::back_inserter(places),
		                       &Footpath::Node::place);
		return places;
	}

	[[nodiscard]] size_t CountOfPaths() const
	{
		size_t count = 0;
		_world->registry.Each<const Footpath>([&count](entt::entity, const Footpath&) { ++count; });
		return count;
	}

	[[nodiscard]] size_t CountOfFinders() const
	{
		size_t count = 0;
		_world->registry.Each<const FootpathFinder>([&count](entt::entity, const FootpathFinder&) { ++count; });
		return count;
	}

	/// No node that walkers use stands inside the circle
	[[nodiscard]] bool WalkedNodesClearOf(entt::entity footpath, glm::vec3 centre, float radius) const
	{
		const auto& nodes = _world->registry.Get<const Footpath>(footpath).nodes;
		return std::ranges::all_of(nodes, [centre, radius](const Footpath::Node& node) {
			return node.hidden || node.marker || glm::length(planning::ToPoint(node.place) - glm::xz(centre)) >= radius;
		});
	}

	FakeWorld* _world {nullptr};
	std::unique_ptr<FootpathSystem> _system;
	entt::entity _town {entt::null};
	entt::entity _pit {entt::null};
};

TEST_F(FootpathSystemTest, AWorkingBuildingGetsAPathFromThePitRoundWhatCameBeforeIt)
{
	const auto rock = _world->Make(k_RockPosition, k_RockRadius);
	const auto house = MakeAbode(k_HousePosition);
	_system->BuildingWorking(house);

	// The map synced before the path was planned, so the rock is filed and the path goes round it
	EXPECT_TRUE(_world->filed.contains(rock));
	const auto paths = PathsOf(house);
	ASSERT_EQ(paths.size(), 1U);
	EXPECT_EQ(PathsOf(_pit), paths);
	const auto places = PlacesOf(paths.front());
	ASSERT_EQ(places.size(), 3U);
	EXPECT_EQ(places.front(), _world->ArrivePlace(_pit));
	EXPECT_EQ(places.back(), _world->ArrivePlace(house));
	EXPECT_TRUE(WalkedNodesClearOf(paths.front(), k_RockPosition, k_RockRadius));
	// The rock's change came before the path, so the path is not sent round it again: one search only
	EXPECT_EQ(_world->searches, 1);
	EXPECT_TRUE(_world->registry.Context().footpathObstacleChanges.empty());
	EXPECT_TRUE(std::ranges::none_of(_world->registry.Get<const Footpath>(paths.front()).nodes,
	                                 [](const Footpath::Node& node) { return node.hidden || node.marker; }));
}

TEST_F(FootpathSystemTest, ThePathEndsAndTheMarkerOfAThingHaveTheHeightsOfTheirPlaces)
{
	const auto house = MakeAbode(k_HousePosition);
	_world->arriveAltitudes[_pit] = 0.25f;
	_world->arriveAltitudes[house] = 0.75f;
	_system->BuildingWorking(house);
	const auto path = PathsOf(house).front();
	{
		const auto& nodes = _world->registry.Get<const Footpath>(path).nodes;
		ASSERT_EQ(nodes.size(), 2U);
		EXPECT_EQ(nodes.front().altitude, 0.25f);
		EXPECT_EQ(nodes.back().altitude, 0.75f);
		// A node is drawn above the land by its height
		EXPECT_EQ(nodes.back().position.y, 0.75f);
	}

	// A thing below the land put on the path: the marker at its place has its height, the route's nodes the height 0
	const auto rock = _world->Make(k_RockPosition, k_RockRadius);
	_world->arriveAltitudes[rock] = -0.5f;
	_world->SyncMap();
	_system->ProcessObstacleChanges();
	const auto& nodes = _world->registry.Get<const Footpath>(path).nodes;
	ASSERT_GT(nodes.size(), 2U);
	EXPECT_EQ(std::ranges::count_if(nodes, &Footpath::Node::marker), 1);
	EXPECT_EQ(nodes.front().altitude, 0.25f);
	EXPECT_EQ(nodes.back().altitude, 0.75f);
	for (size_t i = 1; i + 1 < nodes.size(); ++i)
	{
		EXPECT_EQ(nodes[i].altitude, nodes[i].marker ? -0.5f : 0.0f);
	}
}

TEST_F(FootpathSystemTest, AMiracleDispenserOfATownGetsAPathFromThePitAsAnAbodeDoes)
{
	const auto dispenser = _world->Make(k_HousePosition);
	_world->registry.Assign<SpellDispenser>(dispenser);
	_world->towns[dispenser] = _town;
	_system->BuildingWorking(dispenser);

	const auto paths = PathsOf(dispenser);
	ASSERT_EQ(paths.size(), 1U);
	EXPECT_EQ(PathsOf(_pit), paths);
	const auto places = PlacesOf(paths.front());
	EXPECT_EQ(places.front(), _world->ArrivePlace(_pit));
	EXPECT_EQ(places.back(), _world->ArrivePlace(dispenser));

	// A thing that is neither an abode nor a dispenser gets no path
	const auto rock = _world->Make(k_RockPosition, k_RockRadius);
	_world->towns[rock] = _town;
	_system->BuildingWorking(rock);
	EXPECT_TRUE(PathsOf(rock).empty());
}

TEST_F(FootpathSystemTest, NoPathAtTheFirstTurnForThePitItselfOrWithoutABuiltPit)
{
	const auto house = MakeAbode(k_HousePosition);
	_world->turn = 0;
	_system->BuildingWorking(house);
	EXPECT_TRUE(PathsOf(house).empty());

	_world->turn = 1;
	_system->BuildingWorking(_pit);
	EXPECT_TRUE(PathsOf(_pit).empty());

	_world->unbuilt.insert(_pit);
	_system->BuildingWorking(house);
	EXPECT_TRUE(PathsOf(house).empty());

	_world->unbuilt.clear();
	_world->findsWay = false;
	_system->BuildingWorking(house);
	EXPECT_TRUE(PathsOf(house).empty());
	EXPECT_EQ(CountOfPaths(), 0U);
}

TEST_F(FootpathSystemTest, AThingPutOnAPathSendsItRoundWhenTheChangesAreProcessed)
{
	const auto house = MakeAbode(k_HousePosition);
	_system->BuildingWorking(house);
	const auto path = PathsOf(house).front();
	const auto straight = PlacesOf(path);
	ASSERT_EQ(straight.size(), 2U);

	_world->Make(k_RockPosition, k_RockRadius);
	_world->SyncMap();
	// The change waits for the next process, as the turn does it after the step that made the thing
	EXPECT_EQ(PlacesOf(path), straight);
	EXPECT_EQ(_world->registry.Context().footpathObstacleChanges.size(), 1U);

	_system->ProcessObstacleChanges();
	EXPECT_NE(PlacesOf(path), straight);
	EXPECT_TRUE(WalkedNodesClearOf(path, k_RockPosition, k_RockRadius));
	EXPECT_EQ(PlacesOf(path).front(), straight.front());
	EXPECT_EQ(PlacesOf(path).back(), straight.back());
	EXPECT_TRUE(_world->registry.Context().footpathObstacleChanges.empty());
}

TEST_F(FootpathSystemTest, AThingTakenOffThePathBringsItBack)
{
	const auto house = MakeAbode(k_HousePosition);
	_system->BuildingWorking(house);
	const auto path = PathsOf(house).front();
	const auto straight = PlacesOf(path);

	const auto rock = _world->Make(k_RockPosition, k_RockRadius);
	_world->SyncMap();
	_system->ProcessObstacleChanges();
	ASSERT_NE(PlacesOf(path), straight);

	_world->TakeOut(rock);
	_system->ProcessObstacleChanges();
	EXPECT_EQ(PlacesOf(path), straight);
	EXPECT_TRUE(_world->registry.Context().footpathObstacles.empty());
}

TEST_F(FootpathSystemTest, AThingThePathsGoThroughChangesNoPath)
{
	const auto house = MakeAbode(k_HousePosition);
	_system->BuildingWorking(house);
	const auto path = PathsOf(house).front();
	const auto straight = PlacesOf(path);

	const auto bush = _world->Make(k_RockPosition);
	_world->SyncMap();
	EXPECT_TRUE(_world->registry.Context().footpathObstacleChanges.empty());
	_world->TakeOut(bush);
	EXPECT_TRUE(_world->registry.Context().footpathObstacleChanges.empty());
	_system->ProcessObstacleChanges();
	EXPECT_EQ(PlacesOf(path), straight);
}

TEST_F(FootpathSystemTest, AThingFarFromThePathLeavesItAsItIs)
{
	const auto house = MakeAbode(k_HousePosition);
	_system->BuildingWorking(house);
	const auto path = PathsOf(house).front();
	const auto straight = PlacesOf(path);
	const int searchesBefore = _world->searches;

	_world->Make(k_RockPosition + glm::vec3(0.0f, 0.0f, 200.0f), k_RockRadius);
	_world->SyncMap();
	_system->ProcessObstacleChanges();
	EXPECT_EQ(PlacesOf(path), straight);
	EXPECT_EQ(_world->searches, searchesBefore);
}

TEST_F(FootpathSystemTest, AWalkerOnAChangedStretchFindsItsWayOnTheNewNodes)
{
	const auto house = MakeAbode(k_HousePosition);
	_system->BuildingWorking(house);
	const auto path = PathsOf(house).front();

	// A villager walks from the pit to the house, heading for the last node
	const auto villager = _world->registry.Create();
	_world->registry.Assign<LivingAction>(villager, VillagerStates::MoveOnPath, static_cast<uint16_t>(0));
	_world->registry.Assign<FootpathWalk>(villager, path, false, std::optional<uint32_t>(1), entt::null);

	_world->Make(k_RockPosition, k_RockRadius);
	_world->SyncMap();
	_system->ProcessObstacleChanges();

	const auto& walk = _world->registry.Get<const FootpathWalk>(villager);
	const auto places = PlacesOf(path);
	ASSERT_TRUE(walk.node.has_value());
	ASSERT_LT(*walk.node, places.size());
	// It still walks to the house's end of the path, which is now further along the nodes
	EXPECT_EQ(places[*walk.node], places.back());
	EXPECT_GT(*walk.node, 1U);
}

TEST_F(FootpathSystemTest, ATownJoiningASiteFindsItsPathOverTurnsAndLaysItTheTurnAfter)
{
	const auto site = _world->Make(k_HousePosition);
	_world->SyncMap();
	_world->stepsToFind = planning::k_StepsPerTurn * 2 + 5;
	_system->TownJoinedSite(site, _town);

	ASSERT_EQ(CountOfPaths(), 1U);
	EXPECT_TRUE(PathsOf(site).empty());
	EXPECT_TRUE(PathsOf(_pit).empty());
	// Three turns to find the way, then it is laid on the fourth
	for (int i = 0; i < 3; ++i)
	{
		_system->ProcessTurn();
		EXPECT_TRUE(PathsOf(site).empty()) << "turn " << i;
	}
	_system->ProcessTurn();
	const auto paths = PathsOf(site);
	ASSERT_EQ(paths.size(), 1U);
	EXPECT_EQ(PathsOf(_pit), paths);
	EXPECT_EQ(PlacesOf(paths.front()).front(), _world->ArrivePlace(_pit));
	EXPECT_EQ(PlacesOf(paths.front()).back(), _world->ArrivePlace(site));
	EXPECT_TRUE(CountOfFinders() == 0);

	// A town that joins again finds the site's path already from its pit
	_system->TownJoinedSite(site, _town);
	EXPECT_TRUE(CountOfFinders() == 0);
	EXPECT_EQ(CountOfPaths(), 1U);
}

TEST_F(FootpathSystemTest, AFinderWhoseEndGoesOrThatFindsNoWayTakesItsPathAway)
{
	const auto site = _world->Make(k_HousePosition);
	_world->SyncMap();
	_world->stepsToFind = planning::k_StepsPerTurn * 3;
	_system->TownJoinedSite(site, _town);
	_system->ProcessTurn();
	_world->registry.Destroy(site);
	_system->ProcessTurn();
	EXPECT_EQ(CountOfPaths(), 0U);
	EXPECT_TRUE(CountOfFinders() == 0);

	const auto other = _world->Make(k_HousePosition);
	_world->SyncMap();
	_world->findsWay = false;
	_world->stepsToFind = 1;
	_system->TownJoinedSite(other, _town);
	// The turn that the search fails keeps the path; the next turn takes it away
	_system->ProcessTurn();
	EXPECT_EQ(CountOfPaths(), 1U);
	_system->ProcessTurn();
	EXPECT_EQ(CountOfPaths(), 0U);
	EXPECT_TRUE(PathsOf(other).empty());
}

TEST_F(FootpathSystemTest, OnlyADanceAwayFromSitesAndTemplesGetsAPathFromTheNearestTown)
{
	const auto makeDance = [this](DanceInfo type) {
		const auto dance = _world->registry.Create();
		auto& data = _world->registry.Assign<Dance>(dance);
		data.type = type;
		data.place = {
		    .x = map_coords::ToFixed(k_HousePosition.x), .z = map_coords::ToFixed(k_HousePosition.z), .altitude = 0.0f};
		_world->arrive[dance] = wall_hug::ToWhole(glm::xz(k_HousePosition));
		return dance;
	};
	const auto worship = makeDance(DanceInfo::Worship);
	_system->DanceMade(worship, _town);
	const auto temple = makeDance(DanceInfo::CitadelDance_1);
	_system->DanceMade(temple, _town);
	EXPECT_EQ(CountOfPaths(), 0U);

	const auto dance = makeDance(DanceInfo::DanceTownCelebration);
	_system->DanceMade(dance, entt::null);
	const auto paths = PathsOf(dance);
	ASSERT_EQ(paths.size(), 1U);
	EXPECT_EQ(PathsOf(_pit), paths);
	// The path runs from the dance to the pit
	EXPECT_EQ(PlacesOf(paths.front()).front(), _world->ArrivePlace(dance));
	EXPECT_EQ(PlacesOf(paths.front()).back(), _world->ArrivePlace(_pit));
	// A dance has no body, so it keeps its paths where they end
	const auto& link = _world->registry.Get<const FootpathLink>(dance);
	EXPECT_FLOAT_EQ(link.position.x, planning::ToPoint(_world->ArrivePlace(dance)).x);
}

} // namespace

// A load of a saved game of the game puts the things that it made on the map, but sends no path round them. All the
// paths, links, searches and walks go, and the paths of the file take their place with their nodes as given.
TEST_F(FootpathSystemTest, ALoadedGamePutsItsPathsAndSendsNoPathRoundTheThingsThatItMade)
{
	const auto house = MakeAbode(k_HousePosition);
	_system->BuildingWorking(house);
	const auto site = _world->Make(glm::vec3 {100.0f, 0.0f, 140.0f});
	_world->SyncMap();
	_world->stepsToFind = planning::k_StepsPerTurn * 3;
	_system->TownJoinedSite(site, _town);
	ASSERT_EQ(CountOfFinders(), 1U);
	const auto villager = _world->registry.Create();
	_world->registry.Assign<LivingAction>(villager, VillagerStates::MoveOnPath, static_cast<uint16_t>(0));
	_world->registry.Assign<FootpathWalk>(villager, PathsOf(house).front(), false, std::optional<uint32_t>(1), entt::null);

	// The load makes a rock on the line between the pit and the house, which waits to go onto the map
	_world->Make(k_RockPosition, k_RockRadius);
	const auto pitPlace = wall_hug::ToWhole(glm::xz(k_PitPosition));
	const auto housePlace = wall_hug::ToWhole(glm::xz(k_HousePosition));
	Footpath file {.walkable = true};
	file.nodes.push_back({.position = k_PitPosition, .place = pitPlace});
	file.nodes.push_back({.position = k_RockPosition, .place = wall_hug::ToWhole(glm::xz(k_RockPosition)), .hidden = true});
	file.nodes.push_back({.position = k_HousePosition, .place = housePlace, .planned = true});
	Footpath unwalked {.walkable = false};
	unwalked.nodes.push_back({.position = k_HousePosition, .place = housePlace, .marker = true});
	const std::array loaded {file, unwalked};
	const auto made = _system->PutLoadedFootpaths(loaded);

	ASSERT_EQ(made.size(), 2U);
	EXPECT_EQ(CountOfPaths(), 2U);
	EXPECT_EQ(CountOfFinders(), 0U);
	EXPECT_TRUE(PathsOf(house).empty());
	EXPECT_TRUE(PathsOf(_pit).empty());
	EXPECT_FALSE(_world->registry.AllOf<FootpathWalk>(villager));
	// The rock is on the map, and no change waits for the paths
	EXPECT_TRUE(_world->registry.Context().footpathObstacleChanges.empty());
	EXPECT_EQ(_world->registry.Context().footpathObstacles.size(), 1U);
	const auto& first = _world->registry.Get<const Footpath>(made[0]);
	EXPECT_TRUE(first.walkable);
	ASSERT_EQ(first.nodes.size(), 3U);
	EXPECT_EQ(PlacesOf(made[0]), (std::vector<glm::ivec2> {pitPlace, file.nodes[1].place, housePlace}));
	EXPECT_TRUE(first.nodes[1].hidden);
	EXPECT_TRUE(first.nodes[2].planned);
	EXPECT_EQ(first.nodes[2].position, k_HousePosition);
	const auto& second = _world->registry.Get<const Footpath>(made[1]);
	EXPECT_FALSE(second.walkable);
	ASSERT_EQ(second.nodes.size(), 1U);
	EXPECT_TRUE(second.nodes[0].marker);

	// A later turn leaves the loaded paths as the file gave them
	_system->ProcessTurn();
	EXPECT_EQ(PlacesOf(made[0]), (std::vector<glm::ivec2> {pitPlace, file.nodes[1].place, housePlace}));
}
