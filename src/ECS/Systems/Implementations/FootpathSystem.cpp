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
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

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
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureObstacleThings.h"
#include "ECS/FootpathObstacles.h"
#include "ECS/FootpathPlanning.h"
#include "ECS/FootpathRules.h"
#include "ECS/ListOrder.h"
#include "ECS/Map.h"
#include "ECS/PlayerTowns.h"
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
using footpath_world::Plan;
using footpath_world::RouteStatus;
using footpath_world::WorldInterface;

namespace
{

/// A route that no one walks keeps no margin round what it goes round
constexpr float k_RouteMargin = 0.0f;
/// Only a dance that is not a worship site's or a temple's gets a path
constexpr auto k_FirstTempleDance = DanceInfo::CitadelDance_1;
constexpr auto k_LastTempleDance = DanceInfo::CitadelDance_6;
/// A site already has the path from a pit when one of its paths ends this near the site's gate
constexpr float k_SitePathReach = 0.001f;

/// The game's own world, through the game's systems
class GameWorld final: public WorldInterface
{
public:
	[[nodiscard]] Registry& Entities() override { return Locator::entitiesRegistry::value(); }

	[[nodiscard]] uint32_t Turn() const override
	{
		return Locator::time::has_value() ? static_cast<uint32_t>(Locator::time::value().GetTurn()) : 0;
	}

	void SyncMap() override
	{
		// Only the things made go in: a thing that moved is in its new cells already, as the game moves it
		if (Locator::entitiesMap::has_value())
		{
			Locator::entitiesMap::value().FileNew();
		}
	}

	/// Where a path to a thing ends: a building's door, the middle of a crèche, a graveyard, a town centre or a wonder, a
	/// worship site's gate, half a cell along from a dance's place, or else the thing's place
	[[nodiscard]] glm::ivec2 ArrivePlace(entt::entity thing) const override
	{
		/// A dance's footpaths end half a cell along x from its place
		constexpr int32_t k_DanceArriveOffset = 0x8000;
		const auto& registry = SharedRegistry();
		if (const auto* dance = registry.TryGet<const Dance>(thing))
		{
			return {dance->place.x + k_DanceArriveOffset, dance->place.z};
		}
		if (registry.AllOf<WorshipSite>(thing) && Locator::worshipSiteSystem::has_value())
		{
			if (const auto gate =
			        Locator::worshipSiteSystem::value().PlaceAt(thing, WorshipSiteSystemInterface::SitePlace::Gate))
			{
				return wall_hug::ToWhole(*gate);
			}
			return PlaceOf(thing);
		}
		// A miracle dispenser is reached at its door, as an abode
		if (registry.AnyOf<Abode, SpellDispenser>(thing))
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

	/// The first end towards the second as it was, and then the second towards the first as moved
	[[nodiscard]] std::pair<glm::vec2, glm::vec2> PlannedEnds(entt::entity from, entt::entity to) const override
	{
		const auto& registry = SharedRegistry();
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

	[[nodiscard]] footpath_world::ObstacleLook ObstacleOf(entt::entity entity, glm::vec3 /*position*/) const override
	{
		const auto& registry = SharedRegistry();
		const auto thing = creature_obstacle_things::ThingOf(registry, entity);
		footpath_world::ObstacleLook look {.known = thing.has_value()};
		if (thing.has_value() && obstacles::MustAvoidWithoutWalker(*thing))
		{
			look.radius = systems::object_measures::TwoDRadius(registry, entity);
		}
		return look;
	}

	[[nodiscard]] Plan StartRoute(glm::vec2 start, glm::vec2 goal) const override
	{
		Plan plan {.obstacles = {}, .search = std::nullopt, .start = start, .goal = goal, .live = false};
		plan.obstacles.ResetWithoutWalker(k_RouteMargin);
		MakeLive(plan);
		plan.search.emplace(plan.obstacles, route::Search::Start {.point = start},
		                    route::Search::Goal {.point = goal, .mostCost = planning::MostCost(start, goal)});
		return plan;
	}

	/// Lets the obstacles of a plan fill each block that the search reaches, and leave out the circles that hold either end
	void MakeLive(Plan& plan) const override
	{
		plan.obstacles.SetGatherer(Gatherer());
		plan.obstacles.SetBlockGatherer(BlockGatherer(_gathered));
		// Nothing that holds either end is gone round
		plan.obstacles.LeaveOutCirclesHolding(plan.goal, plan.start);
		plan.live = true;
	}

	void StepRoute(Plan& plan) const override { plan.search->Step(plan.obstacles); }

	[[nodiscard]] RouteStatus StatusOfRoute(const Plan& plan) const override
	{
		switch (plan.search->GetStatus())
		{
		case route::Search::Status::Searching:
			return RouteStatus::Searching;
		case route::Search::Status::Found:
			return RouteStatus::Found;
		default:
			return RouteStatus::Failed;
		}
	}

	[[nodiscard]] std::optional<std::vector<glm::ivec2>> RouteOf(const Plan& plan) const override
	{
		if (plan.search->GetStatus() != route::Search::Status::Found)
		{
			return std::nullopt;
		}
		return planning::RoutePlaces(plan.search->GetBest(), plan.obstacles.All(), plan.start, plan.goal);
	}

	[[nodiscard]] entt::entity TownOf(entt::entity building) const override
	{
		const auto& registry = SharedRegistry();
		if (const auto* dispenser = registry.Valid(building) ? registry.TryGet<const SpellDispenser>(building) : nullptr)
		{
			return registry.Valid(dispenser->town) && registry.AllOf<Town>(dispenser->town) ? dispenser->town : entt::null;
		}
		return town_homes::TownOfAbode(registry, building);
	}

	[[nodiscard]] std::optional<entt::entity> StoragePitOf(entt::entity town) const override
	{
		return construction::StoragePitOf(town);
	}

	[[nodiscard]] bool IsBuilt(entt::entity building) const override { return construction::IsBuilt(building); }

	[[nodiscard]] float LandHeightAt(glm::vec2 point) const override
	{
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
	}

	[[nodiscard]] float AltitudeAt(glm::ivec2 place) const override
	{
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetAltitude(place.x, place.y) : 0.0f;
	}

	void LookAgain(entt::entity walker) override
	{
		if (Locator::pathfindingSystem::has_value())
		{
			Locator::pathfindingSystem::value().LookAgain(walker);
		}
	}

	void WalkTo(entt::entity walker, glm::ivec2 place) override
	{
		if (Locator::pathfindingSystem::has_value())
		{
			Locator::pathfindingSystem::value().WalkTo(walker, place);
		}
	}

	void KeepGathered(bool keep) override
	{
		_gathered->keep = keep;
		if (_gathered->kept)
		{
			std::ranges::fill(_gathered->blocks, std::nullopt);
			_gathered->circles.clear();
			_gathered->kept = false;
		}
	}

private:
	/// The circles of each block that a plan gathered, while the world keeps them
	struct Gathered
	{
		bool keep {false};
		/// Some blocks or circles are kept
		bool kept {false};
		/// The circles of each thing to go round
		std::unordered_map<entt::entity, std::vector<route::Circle>> circles;
		std::vector<std::optional<route::BlockGathering>> blocks = std::vector<std::optional<route::BlockGathering>>(
		    static_cast<size_t>(route::Obstacles::k_BlocksPerSide * route::Obstacles::k_BlocksPerSide));
	};

	[[nodiscard]] static const ecs::Registry& SharedRegistry() { return Locator::entitiesRegistry::value(); }

	[[nodiscard]] static glm::ivec2 PlaceOf(entt::entity thing)
	{
		return wall_hug::ToWhole(glm::xz(SharedRegistry().Get<const Transform>(thing).position));
	}

	/// The circles of a thing that a route no one walks goes round, none for a thing it goes through
	[[nodiscard]] static std::vector<route::Circle> CirclesToGoRound(const ecs::Registry& registry, entt::entity entity)
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

	/// What stands in each cell of a block, in the order of the gatherer of each cell. While the world keeps what the plans
	/// gather, a block gathered before gives its kept circles.
	[[nodiscard]] static route::Obstacles::BlockGatherer BlockGatherer(std::shared_ptr<Gathered> gathered)
	{
		return [gathered = std::move(gathered)](glm::ivec2 block, route::Obstacles& into) {
			const auto index = static_cast<size_t>((block.y * route::Obstacles::k_BlocksPerSide) + block.x);
			if (gathered->keep && gathered->blocks[index].has_value())
			{
				gathered->blocks[index]->AddTo(into);
				return;
			}
			route::BlockGathering gathering;
			const auto* land = Locator::creatureLocomotionSystem::has_value()
			                       ? &Locator::creatureLocomotionSystem::value().GetWalkableLand()
			                       : nullptr;
			const auto* map = Locator::entitiesMap::has_value() ? &Locator::entitiesMap::value() : nullptr;
			for (int32_t cz = 0; cz < route::Obstacles::k_CellsPerBlock; ++cz)
			{
				for (int32_t cx = 0; cx < route::Obstacles::k_CellsPerBlock; ++cx)
				{
					GatherCell({(block.x * route::Obstacles::k_CellsPerBlock) + cx,
					            (block.y * route::Obstacles::k_CellsPerBlock) + cz},
					           land, map, *gathered, gathering);
				}
			}
			gathering.AddTo(into);
			if (gathered->keep)
			{
				gathered->blocks[index] = std::move(gathering);
				gathered->kept = true;
			}
		};
	}

	/// What stands in a cell: the land that cannot be walked on and the water, then each thing to go round. While the
	/// world keeps what the plans gather, it keeps the circles of each thing too.
	static void GatherCell(glm::ivec2 cell, const route::WalkableLand* land, const MapInterface* map, Gathered& gathered,
	                       route::BlockGathering& gathering)
	{
		if (land != nullptr)
		{
			route::AddLandCircles(*land, cell, gathering, true);
		}
		if (map == nullptr)
		{
			return;
		}
		const auto& registry = SharedRegistry();
		for (const auto entity : map->GetAllInCell(cell))
		{
			if (!gathered.keep)
			{
				const auto circles = CirclesToGoRound(registry, entity);
				if (!circles.empty())
				{
					gathering.Thing(cell, entt::to_integral(entity), circles);
				}
				continue;
			}
			auto found = gathered.circles.find(entity);
			if (found == gathered.circles.end())
			{
				found = gathered.circles.emplace(entity, CirclesToGoRound(registry, entity)).first;
				gathered.kept = true;
			}
			if (!found->second.empty())
			{
				gathering.Thing(cell, entt::to_integral(entity), found->second);
			}
		}
	}

	/// What stands in a cell, added the first time the search reaches the cell's block: the land that can't be walked on
	/// and the water, then each thing to go round whose circles aren't filed in the block already
	[[nodiscard]] static route::Obstacles::Gatherer Gatherer()
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
			const auto& registry = SharedRegistry();
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

	std::shared_ptr<Gathered> _gathered = std::make_shared<Gathered>();
};

/// Takes the search up to a number of steps further, stopping once it ends
void StepPlan(const WorldInterface& world, FootpathFinder::Plan& plan, int32_t steps)
{
	for (int32_t i = 0; i < steps && world.StatusOfRoute(plan) == RouteStatus::Searching; ++i)
	{
		world.StepRoute(plan);
	}
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

/// The town nearest a place, however far
entt::entity NearestTown(const Registry& registry, glm::ivec2 place)
{
	const map_coords::MapCoords from {.x = place.x, .z = place.y, .altitude = 0.0f};
	entt::entity nearest = entt::null;
	float best = std::numeric_limits<float>::max();
	// Each player's town list in turn and the neutral list last: the first met wins a tie
	for (const auto town : player_towns::AllInOrder(registry))
	{
		const auto* transform = registry.TryGet<const Transform>(town);
		if (transform == nullptr)
		{
			continue;
		}
		const float distance = gutils::GetDistanceInMetres(from, map_coords::MapCoords {
		                                                             .x = map_coords::ToFixed(transform->position.x),
		                                                             .z = map_coords::ToFixed(transform->position.z),
		                                                             .altitude = 0.0f,
		                                                         });
		if (distance < best)
		{
			best = distance;
			nearest = town;
		}
	}
	return nearest;
}

std::vector<footpath_rules::Path> PathsOf(const Registry& registry, entt::entity thing,
                                          std::vector<std::vector<glm::ivec2>>& places)
{
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
std::optional<FootpathObstacleChange> ObstacleOf(const WorldInterface& world, entt::entity entity, glm::vec3 position, bool put,
                                                 bool& known)
{
	const auto look = world.ObstacleOf(entity, position);
	known = look.known;
	if (!look.radius.has_value())
	{
		return std::nullopt;
	}
	return FootpathObstacleChange {.put = put, .radius = *look.radius, .place = wall_hug::ToWhole(glm::xz(position))};
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

FootpathSystem::FootpathSystem()
    : _world(std::make_unique<GameWorld>())
{
}

FootpathSystem::FootpathSystem(std::unique_ptr<WorldInterface> world)
    : _world(std::move(world))
{
}

FootpathSystem::~FootpathSystem() = default;

void FootpathSystem::SetNodes(Footpath& footpath, std::span<const planning::PlanNode> nodes) const
{
	footpath.nodes.clear();
	footpath.nodes.reserve(nodes.size());
	for (const auto& node : nodes)
	{
		const auto ground = planning::ToPoint(node.place);
		footpath.nodes.push_back({.position = {ground.x, _world->LandHeightAt(ground), ground.y},
		                          .place = node.place,
		                          .planned = node.planned,
		                          .hidden = node.hidden,
		                          .marker = node.marker});
	}
}

entt::entity FootpathSystem::MakeFootpath(glm::ivec2 first, glm::ivec2 last)
{
	auto& registry = _world->Entities();
	const auto entity = registry.Create();
	auto& footpath = registry.Assign<Footpath>(entity);
	const std::array<planning::PlanNode, 2> ends {planning::PlanNode {.place = first}, planning::PlanNode {.place = last}};
	SetNodes(footpath, ends);
	return entity;
}

void FootpathSystem::LayRoute(entt::entity entity, std::span<const glm::ivec2> route, bool settle)
{
	auto& footpath = _world->Entities().Get<Footpath>(entity);
	auto nodes = PlanNodesOf(footpath);
	nodes = planning::LayBetween(nodes, 0, nodes.size() - 1, route, std::nullopt);
	if (settle)
	{
		planning::Settle(nodes);
	}
	SetNodes(footpath, nodes);
}

void FootpathSystem::AddFootpath(entt::entity thing, entt::entity footpath)
{
	auto& registry = _world->Entities();
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
			const auto ground = planning::ToPoint(_world->ArrivePlace(thing));
			position = {ground.x, _world->LandHeightAt(ground), ground.y};
		}
		link = &registry.Assign<FootpathLink>(thing, position, std::vector<Footpath::Id> {});
	}
	link->footpaths.insert(link->footpaths.begin(), static_cast<Footpath::Id>(footpath));
	registry.SetDirty();
}

std::optional<entt::entity> FootpathSystem::StandingStoragePit(entt::entity town) const
{
	const auto pit = _world->StoragePitOf(town);
	return pit.has_value() && _world->IsBuilt(*pit) ? pit : std::nullopt;
}

void FootpathSystem::BuildingWorking(entt::entity building)
{
	auto& registry = _world->Entities();
	// An abode or a miracle dispenser of a town starts to work
	if (!registry.Valid(building) || !registry.AnyOf<Abode, SpellDispenser>(building) || !registry.AllOf<Transform>(building))
	{
		return;
	}
	const auto town = _world->TownOf(building);
	const auto pit = town != entt::null ? StandingStoragePit(town) : std::nullopt;
	if (!pit.has_value() || *pit == building || _world->Turn() == 0)
	{
		return;
	}
	CatchUpWithMap();
	// The path is planned from the pit to the building's door, with no end moved out of its building
	const auto start = _world->ArrivePlace(*pit);
	const auto goal = _world->ArrivePlace(building);
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
	auto& registry = _world->Entities();
	if (!registry.Valid(site) || !registry.Valid(town) || !registry.AllOf<Transform>(site))
	{
		return;
	}
	const auto pit = _world->StoragePitOf(town);
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
	const auto paths = PathsOf(registry, site, places);
	if (footpath_rules::NearestPathToQuick(paths, _world->ArrivePlace(site), _world->ArrivePlace(*pit), k_SitePathReach)
	        .has_value())
	{
		return;
	}
	const auto [start, goal] = _world->PlannedEnds(*pit, site);
	const auto finder = registry.Create();
	registry.Assign<FootpathFinder>(finder, *pit, site, MakeFootpath(_world->ArrivePlace(*pit), _world->ArrivePlace(site)),
	                                _world->StartRoute(start, goal));
	ecs::list_order::Join(registry, finder);
}

void FootpathSystem::DanceMade(entt::entity dance, entt::entity town)
{
	auto& registry = _world->Entities();
	const auto* data = registry.Valid(dance) ? registry.TryGet<const Dance>(dance) : nullptr;
	if (data == nullptr || data->type == DanceInfo::Worship || data->type == DanceInfo::None ||
	    (data->type >= k_FirstTempleDance && data->type <= k_LastTempleDance))
	{
		return;
	}
	if (town == entt::null)
	{
		town = NearestTown(registry, {data->place.x, data->place.z});
	}
	const auto pit = town != entt::null ? StandingStoragePit(town) : std::nullopt;
	if (!pit.has_value())
	{
		return;
	}
	CatchUpWithMap();
	// The path is planned from the dance to the pit, and laid at once
	const auto [start, goal] = _world->PlannedEnds(dance, *pit);
	auto plan = _world->StartRoute(start, goal);
	StepPlan(*_world, plan, planning::k_MostStepsAtOnce);
	const auto route = _world->RouteOf(plan);
	if (!route.has_value())
	{
		return;
	}
	const auto footpath = MakeFootpath(_world->ArrivePlace(dance), _world->ArrivePlace(*pit));
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
	auto& registry = _world->Entities();
	const auto* transform = registry.Valid(thing) ? registry.TryGet<const Transform>(thing) : nullptr;
	if (transform == nullptr)
	{
		return;
	}
	auto& context = registry.Context();
	bool known = false;
	const auto change = ObstacleOf(*_world, thing, transform->position, true, known);
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
	auto& registry = _world->Entities();
	auto& context = registry.Context();
	// The thing as it is now; a thing that is going may not be whole any more, and then it is as it was put on the map
	bool known = false;
	const auto now = ObstacleOf(*_world, thing, position, false, known);
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

bool FootpathSystem::Reroute(entt::entity entity, const FootpathObstacleChange& change,
                             const std::optional<std::pair<glm::ivec2, glm::ivec2>>& nodeSquare)
{
	auto& registry = _world->Entities();
	const auto& footpath = registry.Get<const Footpath>(entity);
	const obstacle_rules::Obstacle obstacle {.radius = change.radius, .place = change.place};
	// A thing put far from every node of the path leaves the path as it is: most things come nowhere near most paths
	if (change.put)
	{
		if (!nodeSquare.has_value() || obstacle_rules::GoesClearOf(nodeSquare->first, nodeSquare->second, obstacle))
		{
			return false;
		}
	}
	// A thing removed far from every node of the path leaves the path as it is too
	else if (const obstacle_rules::ReturnSquare square(obstacle);
	         std::ranges::none_of(footpath.nodes, [&square](const auto& node) { return square.Holds(node.place); }))
	{
		return false;
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
	auto& world = *_world;
	const obstacle_rules::Hooks hooks {
	    .planRoute = [this](glm::ivec2 from, glm::ivec2 to) { return PlanBetween(from, to); },
	    .lookAgain =
	        [&followers, &world](uint32_t key, bool towardsStart) {
		        for (const auto& follower : followers())
		        {
			        if (follower.key == key && follower.towardsStart == towardsStart)
			        {
				        world.LookAgain(follower.villager);
			        }
		        }
	        },
	    .sendOn =
	        [&followers, &world](uint32_t gone, uint32_t back, glm::ivec2 backPlace, uint32_t on, glm::ivec2 onPlace) {
		        for (auto& follower : followers())
		        {
			        if (follower.key != gone)
			        {
				        continue;
			        }
			        follower.key = follower.towardsStart ? back : on;
			        world.WalkTo(follower.villager, follower.towardsStart ? backPlace : onPlace);
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
	    .altitude = [&world](glm::ivec2 place) { return world.AltitudeAt(place); },
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
		return false;
	}
	if (const auto logger = spdlog::get("game"))
	{
		SPDLOG_LOGGER_DEBUG(logger, "Footpath {} {} a thing of radius {} at ({}, {}): {} nodes, then {}",
		                    entt::to_integral(entity), change.put ? "sent round" : "brought back from", change.radius,
		                    map_coords::ToMetres(change.place.x), map_coords::ToMetres(change.place.y), before.size(),
		                    nodes.size());
	}
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
	return true;
}

void FootpathSystem::ProcessObstacleChanges()
{
	auto& registry = _world->Entities();
	// Each change goes through every path. The paths do not depend on each other, so their order does not matter.
	auto changes = std::move(registry.Context().footpathObstacleChanges);
	registry.Context().footpathObstacleChanges.clear();
	if (changes.empty())
	{
		return;
	}
	std::vector<entt::entity> footpaths;
	registry.Each<const Footpath>([&footpaths](entt::entity entity, const Footpath&) { footpaths.push_back(entity); });
	// The code finds the square of the nodes of each path one time, and again only after the path changes
	using NodeSquare = std::optional<std::pair<glm::ivec2, glm::ivec2>>;
	const auto squareOf = [&registry](entt::entity footpath) -> NodeSquare {
		const auto& nodes = registry.Get<const Footpath>(footpath).nodes;
		if (nodes.empty())
		{
			return std::nullopt;
		}
		std::pair<glm::ivec2, glm::ivec2> square {nodes.front().place, nodes.front().place};
		for (const auto& node : nodes)
		{
			square.first = glm::min(square.first, node.place);
			square.second = glm::max(square.second, node.place);
		}
		return square;
	};
	std::vector<NodeSquare> squares;
	squares.reserve(footpaths.size());
	std::ranges::transform(footpaths, std::back_inserter(squares), squareOf);
	for (const auto& change : changes)
	{
		for (size_t i = 0; i < footpaths.size(); ++i)
		{
			if (Reroute(footpaths[i], change, squares[i]))
			{
				squares[i] = squareOf(footpaths[i]);
			}
		}
	}
}

void FootpathSystem::CatchUpWithMap()
{
	_world->SyncMap();
	ProcessObstacleChanges();
}

void FootpathSystem::ProcessTurn()
{
	auto& registry = _world->Entities();
	// The plans share what they gather in the turn. The turn changes only the paths, the links of the paths and the
	// finders, which are not in the map's cells and are not gone round, so what each block holds stays the same.
	struct KeepGathered
	{
		WorldInterface& world;
		explicit KeepGathered(WorldInterface& keeper)
		    : world(keeper)
		{
			world.KeepGathered(true);
		}
		KeepGathered(const KeepGathered&) = delete;
		KeepGathered& operator=(const KeepGathered&) = delete;
		~KeepGathered() { world.KeepGathered(false); }
	} const keepGathered(*_world);
	// A search that a load brought back continues. After the load of a saved game, the search starts again from its two
	// ends, as the game does when it loads a path that it still looks for.
	for (auto&& [entity, finder] : registry.Underlying().view<FootpathFinder>().each())
	{
		if (!finder.plan.has_value())
		{
			if (registry.Valid(finder.from) && registry.Valid(finder.to))
			{
				const auto [start, goal] = _world->PlannedEnds(finder.from, finder.to);
				finder.plan = _world->StartRoute(start, goal);
			}
		}
		else if (!finder.plan->live)
		{
			_world->MakeLive(*finder.plan);
		}
	}
	ProcessObstacleChanges();
	// The finders in the order of the game's list, the newest first
	for (const auto entity : ecs::list_order::Walk<FootpathFinder>(registry))
	{
		auto& finder = registry.Get<FootpathFinder>(entity);
		const bool alive = registry.Valid(finder.from) && registry.Valid(finder.to) && registry.Valid(finder.footpath) &&
		                   finder.plan.has_value();
		const auto status = alive ? _world->StatusOfRoute(*finder.plan) : RouteStatus::Failed;
		if (alive && status == RouteStatus::Searching)
		{
			StepPlan(*_world, *finder.plan, planning::k_StepsPerTurn);
			continue;
		}
		if (alive && status == RouteStatus::Found)
		{
			// The way is found: the path is laid along it and both things keep it
			LayRoute(finder.footpath, *_world->RouteOf(*finder.plan), true);
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
	auto plan = _world->StartRoute(planning::ToPoint(from), planning::ToPoint(to));
	StepPlan(*_world, plan, planning::k_MostStepsAtOnce);
	return _world->RouteOf(plan);
}
