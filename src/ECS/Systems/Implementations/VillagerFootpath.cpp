/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerFootpath.h"

#include <limits>
#include <vector>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WallHug.h"
#include "ECS/FootpathRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/WallHugRules.h"
#include "Locator.h"
#include "VillagerAnimate.h"
#include "VillagerHome.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace villager_footpath = openblack::ecs::villager_footpath;

namespace
{

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

const systems::LivingActionSystemInterface& Living()
{
	return Locator::livingActionSystem::value();
}

glm::vec2 ToMetres(glm::ivec2 whole)
{
	return {map_coords::ToMetres(whole.x), map_coords::ToMetres(whole.y)};
}

/// The town a building belongs to, none for a thing of no town
entt::entity TownOfBuilding(entt::entity building)
{
	const auto& registry = Entities();
	std::optional<uint32_t> id;
	if (const auto* farm = registry.TryGet<const FishFarm>(building))
	{
		return farm->town != entt::null && registry.Valid(farm->town) && registry.AllOf<Town>(farm->town) ? farm->town
		                                                                                                  : entt::null;
	}
	if (const auto* abode = registry.TryGet<const Abode>(building))
	{
		id = abode->townId;
	}
	else if (const auto* field = registry.TryGet<const Field>(building))
	{
		id = static_cast<uint32_t>(field->town);
	}
	if (!id.has_value())
	{
		return entt::null;
	}
	entt::entity found = entt::null;
	registry.Each<const Town>([&](entt::entity town, const Town& data) {
		if (found == entt::null && data.id == *id)
		{
			found = town;
		}
	});
	return found;
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

/// Where a villager going to a building stands when it gets there
glm::ivec2 ArrivePlace(entt::entity building)
{
	const auto& registry = Entities();
	if (const auto* farm = registry.TryGet<const FishFarm>(building))
	{
		return farm->place;
	}
	if (registry.AllOf<Abode>(building))
	{
		return wall_hug::ToWhole(villager_home::ArrivePosition(building));
	}
	return wall_hug::ToWhole(glm::xz(registry.Get<const Transform>(building).position));
}

/// The building's footpaths as the rules see them; the entities alongside, in the same order
struct PathsOf
{
	std::vector<footpath_rules::Path> paths;
	std::vector<entt::entity> entities;
	std::vector<std::vector<glm::ivec2>> places;
};

PathsOf FootpathsOf(entt::entity building)
{
	PathsOf result;
	const auto& registry = Entities();
	const auto* link =
	    building != entt::null && registry.Valid(building) ? registry.TryGet<const FootpathLink>(building) : nullptr;
	if (link == nullptr)
	{
		return result;
	}
	for (const auto id : link->footpaths)
	{
		const auto entity = static_cast<entt::entity>(id);
		const auto* footpath = registry.Valid(entity) ? registry.TryGet<const Footpath>(entity) : nullptr;
		if (footpath == nullptr)
		{
			continue;
		}
		auto& places = result.places.emplace_back();
		places.reserve(footpath->nodes.size());
		for (const auto& node : footpath->nodes)
		{
			places.push_back(node.place);
		}
		result.entities.push_back(entity);
	}
	result.paths.reserve(result.places.size());
	for (size_t i = 0; i < result.places.size(); ++i)
	{
		result.paths.push_back(
		    {.nodes = result.places[i], .walkable = registry.Get<const Footpath>(result.entities[i]).walkable});
	}
	return result;
}

std::vector<glm::ivec2> PlacesOf(entt::entity footpath)
{
	std::vector<glm::ivec2> places;
	const auto& registry = Entities();
	if (footpath == entt::null || !registry.Valid(footpath) || !registry.AllOf<Footpath>(footpath))
	{
		return places;
	}
	for (const auto& node : registry.Get<const Footpath>(footpath).nodes)
	{
		places.push_back(node.place);
	}
	return places;
}

/// The villager joins a path at a node and sets off for it, going round what stands in its way; on the path it walks
/// in the walking-a-path state on to the state it was heading for. Whether the states let it go.
bool SetupMoveOnFootpath(LivingAction& action, entt::entity footpath, bool towardsStart, std::optional<uint32_t> node,
                         VillagerStates final)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	auto& walk = registry.AssignOrReplace<FootpathWalk>(villager);
	walk = FootpathWalk {.footpath = footpath, .towardsStart = towardsStart};
	const auto places = PlacesOf(footpath);
	if (!node.has_value())
	{
		node = footpath_rules::NearestNode(places, villager_home::WalkPosition(villager), towardsStart);
	}
	if (!node.has_value())
	{
		return false;
	}
	walk.node = node;
	// The walk to the first node is laid out before the states change, as the game does
	villager_home::SetupMobileMoveTo(action, ToMetres(places.at(*node)), villager_home::WalkWay::RoundThings);
	if (!Living().VillagerSetCurrentAndDestinationState(action, VillagerStates::MoveOnPath, final))
	{
		return false;
	}
	villager_animate::SetAnim(villager, villager_animate::StateClip(villager), true);
	return true;
}

/// The walk through the building's footpaths, or straight there when none leads its way
bool UseFootpathIfNecessary(LivingAction& action, entt::entity building, glm::ivec2 goal, VillagerStates final)
{
	const auto villager = Entities().ToEntity(action);
	const auto hereGoal = ToMetres(goal);
	const auto buildingPaths = FootpathsOf(building);
	if (buildingPaths.paths.empty() && !Entities().AllOf<FootpathLink>(building))
	{
		return villager_home::SetupMoveTo(action, hereGoal, final);
	}
	const auto here = villager_home::WalkPosition(villager);
	if (const auto join = footpath_rules::NearestPathTo(buildingPaths.paths, here, goal, footpath_rules::k_JoinReach);
	    join.has_value() &&
	    SetupMoveOnFootpath(action, buildingPaths.entities.at(join->path), join->towardsStart, join->node, final))
	{
		return true;
	}
	// No path of the building starts near it: through its town storage pit, along one of the pit's paths and the
	// building's path on from there
	auto town = TownOfBuilding(building);
	if (town == entt::null)
	{
		town = NearestTown(goal);
	}
	const auto pit = town != entt::null ? construction::StoragePitOf(town) : std::nullopt;
	if (pit.has_value() && *pit != building)
	{
		const auto storePaths = FootpathsOf(*pit);
		if (const auto route =
		        footpath_rules::RouteThroughStore(buildingPaths.paths, storePaths.paths, here, goal, ArrivePlace(*pit));
		    route.has_value())
		{
			SetupMoveOnFootpath(action, storePaths.entities.at(route->toStore.path), route->toStore.towardsStart,
			                    route->toStore.node, final);
			if (auto* walk = Entities().TryGet<FootpathWalk>(villager))
			{
				walk->then = buildingPaths.entities.at(route->onward);
			}
			return true;
		}
	}
	return villager_home::SetupMoveTo(action, hereGoal, final);
}

} // namespace

bool villager_footpath::SetupMoveToOnFootpath(LivingAction& action, entt::entity building, glm::vec2 goal, VillagerStates final)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	if (building == entt::null || !registry.Valid(building) || !registry.AllOf<Transform>(building))
	{
		return villager_home::SetupMoveTo(action, goal, final);
	}
	// One standing where it arrives at the building, going on to another point of it, steps straight there
	const auto here = villager_home::WalkPosition(villager);
	const auto whole = wall_hug::ToWhole(goal);
	if (here == ArrivePlace(building) && whole != here)
	{
		return villager_home::SetupMoveTo(action, goal, final);
	}
	return UseFootpathIfNecessary(action, building, whole, final);
}

uint32_t villager_footpath::MoveOnPath(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	auto* walk = registry.TryGet<FootpathWalk>(villager);
	if (walk == nullptr || !registry.AllOf<WallHug>(villager))
	{
		Living().VillagerSetTopStateToFinal(action);
		return 1;
	}
	const auto& wallHug = registry.Get<const WallHug>(villager);
	if (!villager_home::WalkIsOverAt(villager, wallHug.goal))
	{
		return 0;
	}
	const auto here = villager_home::WalkPosition(villager);
	auto places = PlacesOf(walk->footpath);
	// On to the next node; the nodes on the way are walked to a little to one side, the last one exactly
	std::optional<footpath_rules::Leg> leg;
	if (walk->node.has_value())
	{
		leg = footpath_rules::NextLeg(places, *walk->node, walk->towardsStart);
	}
	else if (const auto next = footpath_rules::Rejoin(places, here, walk->towardsStart); next.has_value())
	{
		leg = footpath_rules::Leg {.node = *next,
		                           .jitter = footpath_rules::NextNode(places, *next, walk->towardsStart).has_value()};
	}
	if (leg.has_value())
	{
		walk->node = leg->node;
		auto target = places.at(leg->node);
		if (leg->jitter)
		{
			auto& random = Locator::gameRandom::value();
			const auto drawX = random.GameRand(2 * footpath_rules::k_Jitter);
			const auto drawZ = random.GameRand(2 * footpath_rules::k_Jitter);
			target = footpath_rules::Jittered(target, drawX, drawZ);
		}
		villager_home::SetupMobileMoveTo(action, ToMetres(target), villager_home::WalkWay::StraightThrough);
		return 1;
	}
	// The path has ended: on along the path that follows it, if any, from its end nearer the villager
	if (walk->then != entt::null)
	{
		walk->footpath = walk->then;
		walk->then = entt::null;
		places = PlacesOf(walk->footpath);
		walk->towardsStart = footpath_rules::OnwardTowardsStart(places, here);
		walk->node = footpath_rules::OnwardFirstNode(places, walk->towardsStart);
		if (walk->node.has_value())
		{
			villager_home::SetupMobileMoveTo(action, ToMetres(places.at(*walk->node)), villager_home::WalkWay::StraightThrough);
			return 1;
		}
	}
	// It stands on the path's last node and goes into the state it was heading for
	walk->node.reset();
	registry.Remove<MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
	Living().VillagerSetTopStateToFinal(action);
	return 1;
}

bool villager_footpath::ExitMoveOnPath(LivingAction& action, VillagerStates /*next*/)
{
	auto& registry = Entities();
	registry.Remove<FootpathWalk>(registry.ToEntity(action));
	return true;
}

void villager_footpath::HandOverFootpaths(entt::entity from, entt::entity to)
{
	auto& registry = Entities();
	if (!registry.Valid(from) || !registry.Valid(to))
	{
		return;
	}
	auto* link = registry.TryGet<FootpathLink>(from);
	if (link == nullptr)
	{
		registry.Remove<FootpathLink>(to);
		return;
	}
	registry.AssignOrReplace<FootpathLink>(to, std::move(*link));
	registry.Remove<FootpathLink>(from);
}
