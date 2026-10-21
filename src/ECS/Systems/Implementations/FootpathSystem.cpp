/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "FootpathSystem.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>

#include <glm/common.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureObstacles.h"
#include "Creature/CreatureRouteSearch.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureObstacleThings.h"
#include "ECS/FootpathObstacles.h"
#include "ECS/FootpathPlanning.h"
#include "ECS/FootpathRules.h"
#include "ECS/ListOrder.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "ECS/Systems/PathfindingSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/TownHomes.h"
#include "ECS/WallHugRules.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerHome.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace route = openblack::creature_route;
namespace obstacles = openblack::creature_obstacles;
namespace planning = openblack::ecs::footpath_planning;
namespace obstacle_rules = openblack::ecs::footpath_obstacles;

/// The search for a path's way and what it goes round, kept from turn to turn while the path is found
struct components::FootpathFinder::Plan
{
	route::Obstacles obstacles;
	std::optional<route::Search> search;
	glm::vec2 start {0.0f};
	glm::vec2 goal {0.0f};
};

namespace
{

/// A route that no one walks keeps no margin round what it goes round
constexpr float k_RouteMargin = 0.0f;
/// Only a dance that is not a worship site's or a temple's gets a path
constexpr auto k_FirstTempleDance = DanceInfo::CitadelDance_1;
constexpr auto k_LastTempleDance = DanceInfo::CitadelDance_6;
/// A dance's footpaths end half a cell along x from its place
constexpr int32_t k_DanceArriveOffset = 0x8000;
/// A site already has the path from a pit when one of its paths ends this near the site's gate
constexpr float k_SitePathReach = 0.001f;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

glm::ivec2 PlaceOf(entt::entity thing)
{
	return wall_hug::ToWhole(glm::xz(Entities().Get<const Transform>(thing).position));
}

/// Where a path to a thing ends: a building's door, the middle of a crèche, a graveyard, a town centre or a wonder, a
/// worship site's gate, half a cell along from a dance's place, or else the thing's place
glm::ivec2 ArrivePlace(entt::entity thing)
{
	const auto& registry = Entities();
	if (const auto* dance = registry.TryGet<const Dance>(thing))
	{
		return {dance->place.x + k_DanceArriveOffset, dance->place.z};
	}
	if (registry.AllOf<WorshipSite>(thing) && Locator::worshipSiteSystem::has_value())
	{
		if (const auto gate = Locator::worshipSiteSystem::value().PlaceAt(thing, WorshipSiteSystemInterface::SitePlace::Gate))
		{
			return wall_hug::ToWhole(*gate);
		}
		return PlaceOf(thing);
	}
	if (registry.AllOf<Abode>(thing))
	{
		const auto* info = world_objects::AbodeInfoOf(thing);
		const auto type = info != nullptr ? info->abodeType : AbodeType::General;
		if (type == AbodeType::Creche || type == AbodeType::Graveyard || type == AbodeType::TownCentre ||
		    type == AbodeType::Wonder)
		{
			return PlaceOf(thing);
		}
		return wall_hug::ToWhole(villager_home::ArrivePosition(thing));
	}
	return PlaceOf(thing);
}

/// The circles of a thing that a route no one walks goes round, none for a thing it goes through
std::vector<route::Circle> CirclesToGoRound(const Registry& registry, entt::entity entity)
{
	const auto thing = creature_obstacle_things::ThingOf(registry, entity);
	if (!thing.has_value() || !obstacles::MustAvoidWithoutWalker(*thing))
	{
		return {};
	}
	// The Norse gate is a row of small circles across its width, with a way through the middle while it stands open
	if (registry.AnyOf<AnimatedStatic>(entity) && Locator::animatedStaticSystem::has_value())
	{
		if (const auto gate = Locator::animatedStaticSystem::value().RouteCircles(entity))
		{
			std::vector<route::Circle> circles;
			circles.reserve(gate->size());
			std::ranges::transform(*gate, std::back_inserter(circles), [](const auto& circle) {
				return route::Circle {.centre = circle.centre, .radius = circle.radius};
			});
			return circles;
		}
	}
	return obstacles::CirclesWithoutWalker(*thing, k_RouteMargin);
}

/// What stands in a cell, added the first time the search reaches the cell's block: the land that can't be walked on and
/// the water, then each thing to go round whose circles aren't filed in the block already
route::Obstacles::Gatherer Gatherer()
{
	return [](glm::ivec2 cell, route::Obstacles& into) {
		if (Locator::creatureLocomotionSystem::has_value())
		{
			route::AddLandCircles(Locator::creatureLocomotionSystem::value().GetWalkableLand(), cell, into, true);
		}
		if (!Locator::entitiesMap::has_value())
		{
			return;
		}
		const auto& registry = Entities();
		for (const auto entity : Locator::entitiesMap::value().GetAllInCell(cell))
		{
			const auto key = entt::to_integral(entity);
			if (into.BlockHas(key, cell))
			{
				continue;
			}
			for (const auto& circle : CirclesToGoRound(registry, entity))
			{
				into.Add(key, circle.centre, circle.radius);
			}
		}
	};
}

std::shared_ptr<FootpathFinder::Plan> StartPlan(glm::vec2 start, glm::vec2 goal)
{
	auto plan = std::make_shared<FootpathFinder::Plan>();
	plan->start = start;
	plan->goal = goal;
	plan->obstacles.ResetWithoutWalker(k_RouteMargin, Gatherer());
	// Nothing that holds either end is gone round
	plan->obstacles.LeaveOutCirclesHolding(goal, start);
	plan->search.emplace(plan->obstacles, route::Search::Start {.point = start},
	                     route::Search::Goal {.point = goal, .mostCost = planning::MostCost(start, goal)});
	return plan;
}

/// Takes the search up to a number of steps further, stopping once it ends
void StepPlan(FootpathFinder::Plan& plan, int32_t steps)
{
	for (int32_t i = 0; i < steps && plan.search->GetStatus() == route::Search::Status::Searching; ++i)
	{
		plan.search->Step(plan.obstacles);
	}
}

std::optional<std::vector<glm::ivec2>> RouteOf(const FootpathFinder::Plan& plan)
{
	if (plan.search->GetStatus() != route::Search::Status::Found)
	{
		return std::nullopt;
	}
	return planning::RoutePlaces(plan.search->GetBest(), plan.obstacles.All(), plan.start, plan.goal);
}

std::vector<planning::PlanNode> PlanNodesOf(const Footpath& footpath)
{
	std::vector<planning::PlanNode> nodes;
	nodes.reserve(footpath.nodes.size());
	std::ranges::transform(footpath.nodes, std::back_inserter(nodes), [](const Footpath::Node& node) {
		return planning::PlanNode {.place = node.place, .planned = node.planned, .hidden = node.hidden, .marker = node.marker};
	});
	return nodes;
}

/// The path takes the planned nodes, each drawn on the land
void SetNodes(Footpath& footpath, std::span<const planning::PlanNode> nodes)
{
	const auto* land = Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
	footpath.nodes.clear();
	footpath.nodes.reserve(nodes.size());
	for (const auto& node : nodes)
	{
		const auto ground = planning::ToPoint(node.place);
		footpath.nodes.push_back({.position = {ground.x, land != nullptr ? land->GetHeightAt(ground) : 0.0f, ground.y},
		                          .place = node.place,
		                          .planned = node.planned,
		                          .hidden = node.hidden,
		                          .marker = node.marker});
	}
}

/// A new path with two nodes, the first where the first thing is arrived at
entt::entity MakeFootpath(glm::ivec2 first, glm::ivec2 last)
{
	auto& registry = Entities();
	const auto entity = registry.Create();
	auto& footpath = registry.Assign<Footpath>(entity);
	const std::array<planning::PlanNode, 2> ends {planning::PlanNode {.place = first}, planning::PlanNode {.place = last}};
	SetNodes(footpath, ends);
	return entity;
}

/// The path's nodes between its two ends are laid along the route
void LayRoute(entt::entity entity, std::span<const glm::ivec2> route, bool settle)
{
	auto& footpath = Entities().Get<Footpath>(entity);
	auto nodes = PlanNodesOf(footpath);
	nodes = planning::LayBetween(nodes, 0, nodes.size() - 1, route, std::nullopt);
	if (settle)
	{
		planning::Settle(nodes);
	}
	SetNodes(footpath, nodes);
}

/// A thing keeps a path among those that lead from it, the newest first
void AddFootpath(entt::entity thing, entt::entity footpath)
{
	auto& registry = Entities();
	auto* link = registry.TryGet<FootpathLink>(thing);
	if (link == nullptr)
	{
		// The link is where the thing stands; a dance, which has no body, has it where its paths end
		glm::vec3 position {0.0f};
		if (const auto* transform = registry.TryGet<const Transform>(thing))
		{
			position = transform->position;
		}
		else
		{
			const auto ground = planning::ToPoint(ArrivePlace(thing));
			position = {ground.x,
			            Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(ground) : 0.0f,
			            ground.y};
		}
		link = &registry.Assign<FootpathLink>(thing, position, std::vector<Footpath::Id> {});
	}
	link->footpaths.insert(link->footpaths.begin(), static_cast<Footpath::Id>(footpath));
	registry.SetDirty();
}

/// The two points a path between two things is planned between: where each is arrived at, moved out of its building
/// when it is a building, the first towards the second as it was and then the second towards the first as moved
std::pair<glm::vec2, glm::vec2> PlannedEnds(entt::entity from, entt::entity to)
{
	const auto& registry = Entities();
	auto start = planning::ToPoint(ArrivePlace(from));
	auto goal = planning::ToPoint(ArrivePlace(to));
	const auto clearOf = [&registry](entt::entity building, glm::vec2 end, glm::vec2 other) {
		if (!registry.AnyOf<Abode, WorshipSite>(building))
		{
			return end;
		}
		const auto thing = creature_obstacle_things::ThingOf(registry, building);
		if (!thing.has_value())
		{
			return end;
		}
		const auto circles = obstacles::CirclesWithoutWalker(*thing, planning::k_EndClearMargin);
		return planning::ClearOfBuilding(end, planning::ToPoint(PlaceOf(building)), other, circles);
	};
	start = clearOf(from, start, goal);
	goal = clearOf(to, goal, start);
	return {start, goal};
}

/// The town's storage pit, while it stands built
std::optional<entt::entity> StandingStoragePit(entt::entity town)
{
	const auto pit = construction::StoragePitOf(town);
	return pit.has_value() && construction::IsBuilt(*pit) ? pit : std::nullopt;
}

/// The town nearest a place, however far
entt::entity NearestTown(glm::ivec2 place)
{
	const map_coords::MapCoords from {.x = place.x, .z = place.y, .altitude = 0.0f};
	entt::entity nearest = entt::null;
	float best = std::numeric_limits<float>::max();
	Entities().Each<const Town, const Transform>([&](entt::entity town, const Town&, const Transform& transform) {
		const float distance = gutils::GetDistanceInMetres(from, map_coords::MapCoords {
		                                                             .x = map_coords::ToFixed(transform.position.x),
		                                                             .z = map_coords::ToFixed(transform.position.z),
		                                                             .altitude = 0.0f,
		                                                         });
		if (distance < best)
		{
			best = distance;
			nearest = town;
		}
	});
	return nearest;
}

std::vector<footpath_rules::Path> PathsOf(entt::entity thing, std::vector<std::vector<glm::ivec2>>& places)
{
	const auto& registry = Entities();
	std::vector<bool> walkable;
	if (const auto* link = registry.TryGet<const FootpathLink>(thing))
	{
		for (const auto id : link->footpaths)
		{
			const auto entity = static_cast<entt::entity>(id);
			const auto* footpath = registry.Valid(entity) ? registry.TryGet<const Footpath>(entity) : nullptr;
			if (footpath == nullptr)
			{
				continue;
			}
			auto& nodes = places.emplace_back();
			std::ranges::transform(footpath->nodes, std::back_inserter(nodes), &Footpath::Node::place);
			walkable.push_back(footpath->walkable);
		}
	}
	std::vector<footpath_rules::Path> paths;
	for (size_t i = 0; i < places.size(); ++i)
	{
		paths.push_back({.nodes = places[i], .walkable = walkable[i]});
	}
	return paths;
}

/// What the paths go round of a thing standing at a position: its radius across the ground and its place. None when the
/// creatures' route rule lets them through it. Whether the thing is whole enough to tell goes in known.
std::optional<FootpathObstacleChange> ObstacleOf(const Registry& registry, entt::entity entity, glm::vec3 position, bool put,
                                                 bool& known)
{
	const auto thing = creature_obstacle_things::ThingOf(registry, entity);
	known = thing.has_value();
	if (!thing.has_value() || !obstacles::MustAvoidWithoutWalker(*thing))
	{
		return std::nullopt;
	}
	return FootpathObstacleChange {.put = put,
	                               .radius = systems::object_measures::TwoDRadius(registry, entity),
	                               .place = wall_hug::ToWhole(glm::xz(position))};
}

/// A villager on a path that walks towards one of its nodes, by the node's key while the nodes change
struct Follower
{
	entt::entity villager;
	uint32_t key;
	bool towardsStart;
};

/// The villagers in the MoveOnPath state on a path, each with the node it walks to
std::vector<Follower> FollowersOf(const Registry& registry, entt::entity footpath)
{
	std::vector<Follower> followers;
	registry.Each<const FootpathWalk, const LivingAction>(
	    [&followers, footpath](entt::entity villager, const FootpathWalk& walk, const LivingAction& action) {
		    const auto state = static_cast<VillagerStates>(action.states.at(static_cast<size_t>(LivingAction::Index::Top)));
		    if (walk.footpath == footpath && walk.node.has_value() && state == VillagerStates::MoveOnPath)
		    {
			    followers.push_back({.villager = villager, .key = *walk.node + 1, .towardsStart = walk.towardsStart});
		    }
	    });
	return followers;
}
} // namespace

void FootpathSystem::BuildingWorking(entt::entity building)
{
	auto& registry = Entities();
	if (!registry.Valid(building) || !registry.AllOf<Abode, Transform>(building))
	{
		return;
	}
	const auto town = town_homes::TownOfAbode(registry, building);
	const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	const auto pit = town != entt::null ? StandingStoragePit(town) : std::nullopt;
	if (!pit.has_value() || *pit == building || turn == 0)
	{
		return;
	}
	CatchUpWithMap();
	// The path is planned from the pit to the building's door, with no end moved out of its building
	const auto start = ArrivePlace(*pit);
	const auto goal = ArrivePlace(building);
	const auto route = PlanBetween(start, goal);
	if (!route.has_value())
	{
		return;
	}
	const auto footpath = MakeFootpath(start, goal);
	LayRoute(footpath, *route, false);
	AddFootpath(building, footpath);
	AddFootpath(*pit, footpath);
}

void FootpathSystem::TownJoinedSite(entt::entity site, entt::entity town)
{
	auto& registry = Entities();
	if (!registry.Valid(site) || !registry.Valid(town) || !registry.AllOf<Transform>(site))
	{
		return;
	}
	const auto pit = construction::StoragePitOf(town);
	if (!pit.has_value())
	{
		return;
	}
	CatchUpWithMap();
	if (!registry.AllOf<FootpathLink>(site))
	{
		registry.Assign<FootpathLink>(site, registry.Get<const Transform>(site).position, std::vector<Footpath::Id> {});
	}
	std::vector<std::vector<glm::ivec2>> places;
	const auto paths = PathsOf(site, places);
	if (footpath_rules::NearestPathToQuick(paths, ArrivePlace(site), ArrivePlace(*pit), k_SitePathReach).has_value())
	{
		return;
	}
	const auto [start, goal] = PlannedEnds(*pit, site);
	const auto finder = registry.Create();
	registry.Assign<FootpathFinder>(finder, *pit, site, MakeFootpath(ArrivePlace(*pit), ArrivePlace(site)),
	                                StartPlan(start, goal));
	ecs::list_order::Join(registry, finder);
}

void FootpathSystem::DanceMade(entt::entity dance, entt::entity town)
{
	auto& registry = Entities();
	const auto* data = registry.Valid(dance) ? registry.TryGet<const Dance>(dance) : nullptr;
	if (data == nullptr || data->type == DanceInfo::Worship || data->type == DanceInfo::None ||
	    (data->type >= k_FirstTempleDance && data->type <= k_LastTempleDance))
	{
		return;
	}
	if (town == entt::null)
	{
		town = NearestTown({data->place.x, data->place.z});
	}
	const auto pit = town != entt::null ? StandingStoragePit(town) : std::nullopt;
	if (!pit.has_value())
	{
		return;
	}
	CatchUpWithMap();
	// The path is planned from the dance to the pit, and laid at once
	const auto [start, goal] = PlannedEnds(dance, *pit);
	auto plan = StartPlan(start, goal);
	StepPlan(*plan, planning::k_MostStepsAtOnce);
	const auto route = RouteOf(*plan);
	if (!route.has_value())
	{
		return;
	}
	const auto footpath = MakeFootpath(ArrivePlace(dance), ArrivePlace(*pit));
	LayRoute(footpath, *route, true);
	AddFootpath(*pit, footpath);
	AddFootpath(dance, footpath);
}

void FootpathSystem::DanceMoved(entt::entity dance)
{
	// The game lays the path of a moved dance as it lays that of a new dance with no town
	DanceMade(dance, entt::null);
}

void FootpathSystem::ThingPut(entt::entity thing)
{
	auto& registry = Entities();
	const auto* transform = registry.Valid(thing) ? registry.TryGet<const Transform>(thing) : nullptr;
	if (transform == nullptr)
	{
		return;
	}
	auto& context = registry.Context();
	bool known = false;
	const auto change = ObstacleOf(registry, thing, transform->position, true, known);
	if (!change.has_value())
	{
		context.footpathObstacles.erase(thing);
		return;
	}
	context.footpathObstacles.insert_or_assign(thing, *change);
	context.footpathObstacleChanges.push_back(*change);
}

void FootpathSystem::ThingRemoved(entt::entity thing, glm::vec3 position, bool filed)
{
	auto& registry = Entities();
	auto& context = registry.Context();
	// The thing as it is now; a thing that is going may not be whole any more, and then it is as it was put on the map
	bool known = false;
	const auto now = ObstacleOf(registry, thing, position, false, known);
	const auto kept = context.footpathObstacles.find(thing);
	if (kept == context.footpathObstacles.end())
	{
		// A thing that the paths did not go round when it came, but must go round now, such as a forest on fire
		if (filed && now.has_value())
		{
			context.footpathObstacleChanges.push_back(*now);
		}
		return;
	}
	auto change = kept->second;
	context.footpathObstacles.erase(kept);
	change.put = false;
	change.place = wall_hug::ToWhole(glm::xz(position));
	if (known)
	{
		if (!now.has_value())
		{
			return;
		}
		change.radius = now->radius;
	}
	context.footpathObstacleChanges.push_back(change);
}

void FootpathSystem::Reroute(entt::entity entity, const FootpathObstacleChange& change)
{
	auto& registry = Entities();
	const auto& footpath = registry.Get<const Footpath>(entity);
	const obstacle_rules::Obstacle obstacle {.radius = change.radius, .place = change.place};
	// A thing put far from every node of the path leaves the path as it is: most things come nowhere near most paths
	if (change.put)
	{
		if (footpath.nodes.empty())
		{
			return;
		}
		glm::ivec2 low = footpath.nodes.front().place;
		glm::ivec2 high = low;
		for (const auto& node : footpath.nodes)
		{
			low = glm::min(low, node.place);
			high = glm::max(high, node.place);
		}
		if (obstacle_rules::GoesClearOf(low, high, obstacle))
		{
			return;
		}
	}
	std::vector<obstacle_rules::KeyedNode> nodes;
	nodes.reserve(footpath.nodes.size());
	for (size_t i = 0; i < footpath.nodes.size(); ++i)
	{
		const auto& node = footpath.nodes[i];
		nodes.push_back({.node = {.place = node.place, .planned = node.planned, .hidden = node.hidden, .marker = node.marker},
		                 .key = static_cast<uint32_t>(i + 1)});
	}
	// The walkers on the path are found only when the rules need them
	std::optional<std::vector<Follower>> found;
	const auto followers = [&found, &registry, entity]() -> std::vector<Follower>& {
		if (!found.has_value())
		{
			found = FollowersOf(registry, entity);
		}
		return *found;
	};
	auto* pathfinding = Locator::pathfindingSystem::has_value() ? &Locator::pathfindingSystem::value() : nullptr;
	const auto* land = Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
	const obstacle_rules::Hooks hooks {
	    .planRoute = [this](glm::ivec2 from, glm::ivec2 to) { return PlanBetween(from, to); },
	    .lookAgain =
	        [&followers, pathfinding](uint32_t key, bool towardsStart) {
		        for (const auto& follower : followers())
		        {
			        if (follower.key == key && follower.towardsStart == towardsStart && pathfinding != nullptr)
			        {
				        pathfinding->LookAgain(follower.villager);
			        }
		        }
	        },
	    .sendOn =
	        [&followers, pathfinding](uint32_t gone, uint32_t back, glm::ivec2 backPlace, uint32_t on, glm::ivec2 onPlace) {
		        for (auto& follower : followers())
		        {
			        if (follower.key != gone)
			        {
				        continue;
			        }
			        follower.key = follower.towardsStart ? back : on;
			        if (pathfinding != nullptr)
			        {
				        pathfinding->WalkTo(follower.villager, follower.towardsStart ? backPlace : onPlace);
			        }
		        }
	        },
	    .cutOff =
	        [&followers](uint32_t gone, uint32_t onwardFrom, uint32_t first) {
		        // Each walks on to where the node was, as it did
		        for (auto& follower : followers())
		        {
			        if (follower.key == gone)
			        {
				        follower.key = follower.towardsStart ? first : onwardFrom;
			        }
		        }
	        },
	    .altitude = [land](glm::ivec2 place) { return land != nullptr ? land->GetAltitude(place.x, place.y) : 0.0f; },
	};
	const auto before = nodes;
	if (change.put)
	{
		obstacle_rules::GoRound(nodes, obstacle, hooks);
	}
	else
	{
		obstacle_rules::StopGoingRound(nodes, obstacle, hooks);
	}
	if (nodes == before)
	{
		return;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Footpath {} {} a thing of radius {} at ({}, {}): {} nodes, then {}",
	                    entt::to_integral(entity), change.put ? "sent round" : "brought back from", change.radius,
	                    map_coords::ToMetres(change.place.x), map_coords::ToMetres(change.place.y), before.size(),
	                    nodes.size());
	std::vector<planning::PlanNode> laid;
	laid.reserve(nodes.size());
	std::ranges::transform(nodes, std::back_inserter(laid), &obstacle_rules::KeyedNode::node);
	SetNodes(registry.Get<Footpath>(entity), laid);
	// Each walker walks on to its node where the node now is
	for (const auto& follower : followers())
	{
		auto& walk = registry.Get<FootpathWalk>(follower.villager);
		const auto at = std::ranges::find(nodes, follower.key, &obstacle_rules::KeyedNode::key);
		walk.node = at != nodes.end() ? std::optional(static_cast<uint32_t>(std::distance(nodes.begin(), at))) : std::nullopt;
	}
	registry.SetLayoutDirty();
}

void FootpathSystem::ProcessObstacleChanges()
{
	auto& registry = Entities();
	// Each change goes through every path. The paths do not depend on each other, so their order does not matter.
	auto changes = std::move(registry.Context().footpathObstacleChanges);
	registry.Context().footpathObstacleChanges.clear();
	if (changes.empty())
	{
		return;
	}
	std::vector<entt::entity> footpaths;
	registry.Each<const Footpath>([&footpaths](entt::entity entity, const Footpath&) { footpaths.push_back(entity); });
	for (const auto& change : changes)
	{
		for (const auto footpath : footpaths)
		{
			Reroute(footpath, change);
		}
	}
}

void FootpathSystem::CatchUpWithMap()
{
	if (Locator::entitiesMap::has_value())
	{
		Locator::entitiesMap::value().Sync();
	}
	ProcessObstacleChanges();
}

void FootpathSystem::ProcessTurn()
{
	ProcessObstacleChanges();
	auto& registry = Entities();
	// The finders in the order of the game's list, the newest first
	for (const auto entity : ecs::list_order::Walk<FootpathFinder>(registry))
	{
		auto& finder = registry.Get<FootpathFinder>(entity);
		const bool alive = registry.Valid(finder.from) && registry.Valid(finder.to) && registry.Valid(finder.footpath);
		const auto status = finder.plan->search->GetStatus();
		if (alive && status == route::Search::Status::Searching)
		{
			StepPlan(*finder.plan, planning::k_StepsPerTurn);
			continue;
		}
		if (alive && status == route::Search::Status::Found)
		{
			// The way is found: the path is laid along it and both things keep it
			LayRoute(finder.footpath, *RouteOf(*finder.plan), true);
			AddFootpath(finder.from, finder.footpath);
			AddFootpath(finder.to, finder.footpath);
		}
		else if (registry.Valid(finder.footpath))
		{
			registry.Destroy(finder.footpath);
		}
		registry.Destroy(entity);
	}
}

std::optional<std::vector<glm::ivec2>> FootpathSystem::PlanBetween(glm::ivec2 from, glm::ivec2 to) const
{
	auto plan = StartPlan(planning::ToPoint(from), planning::ToPoint(to));
	StepPlan(*plan, planning::k_MostStepsAtOnce);
	return RouteOf(*plan);
}
