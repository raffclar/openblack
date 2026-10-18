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

#include <glm/gtx/vec_swizzle.hpp>

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
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureObstacleThings.h"
#include "ECS/FootpathPlanning.h"
#include "ECS/FootpathRules.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
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
		return planning::PlanNode {.place = node.place, .planned = node.planned, .hidden = node.hidden, .rejoin = node.rejoin};
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
		                          .rejoin = node.rejoin});
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
		link =
		    &registry.Assign<FootpathLink>(thing, registry.Get<const Transform>(thing).position, std::vector<Footpath::Id> {});
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

void FootpathSystem::ProcessTurn()
{
	auto& registry = Entities();
	std::vector<entt::entity> finders;
	registry.Each<const FootpathFinder>([&finders](entt::entity entity, const FootpathFinder&) { finders.push_back(entity); });
	for (const auto entity : finders)
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
