/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LivingFootpath.h"

#include <glm/vec2.hpp>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Fields.h"
#include "ECS/FishFarms.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/WorshipSite.h"

namespace openblack::ecs::living_footpath
{
using components::LivingFootpath;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

LivingFootpath& FootpathOf(entt::entity living)
{
	auto& registry = Entities();
	if (auto* fp = registry.TryGet<LivingFootpath>(living); fp != nullptr)
	{
		return *fp;
	}
	return registry.Assign<LivingFootpath>(living);
}

/// openblack's walk restarted toward the goal it has: a fresh step and a LINEAR move (the goal untouched)
void RestartLinearMove(entt::entity living, components::WallHug& wallHug)
{
	using namespace components;
	auto& registry = Entities();
	wallHug.step = glm::vec2(0.0f);
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(living);
	registry.Remove<WallHugObjectReference>(living);
	registry.Assign<MoveStateLinearTag>(living);
}

/// The move to a position for openblack's walk: the WallHug goal, a fresh step and a LINEAR move. (approximate) The
/// goal in metres. (pending) MOVE_TO 11 against 12: openblack's wall hug has one LINEAR start
void SetupMobileMoveToPos(entt::entity living, const map_coords::MapCoords& pos, [[maybe_unused]] uint8_t moveTo)
{
	auto* wallHug = Entities().TryGet<components::WallHug>(living);
	if (wallHug == nullptr)
	{
		return;
	}
	wallHug->goal = glm::vec2(map_coords::ToMetres(pos.x), map_coords::ToMetres(pos.z));
	RestartLinearMove(living, *wallHug);
}

/// Out of the occupants' list of its node (every entry of this living)
void LeaveNode(entt::entity living, LivingFootpath& fp)
{
	if (fp.footpath != entt::null && fp.node != footpaths::k_NoNode)
	{
		footpaths::RemoveOccupant(fp.footpath, fp.node, living);
	}
}
} // namespace

map_coords::MapCoords PosOf(entt::entity object)
{
	const auto xz = town_queries::PosOf(object);
	return map_coords::MapCoords {xz.x, xz.y, 0.0f};
}

map_coords::MapCoords ArrivePosOf(entt::entity target, entt::entity living)
{
	auto& registry = Entities();
	if (registry.AllOf<components::BigForest>(target))
	{
		return object::BigForestGetArrivePos(target, living);
	}
	if (registry.AllOf<components::Field>(target))
	{
		return fields::GetArrivePos(target);
	}
	if (registry.AllOf<components::FishFarm>(target))
	{
		return fish_farms::GetArrivePos(target);
	}
	if (registry.AllOf<components::WorshipSite>(target))
	{
		// its special point 9 when it has it, else its position
		if (const auto point = worship::site::GetSpecialPos(target, 9); point.has_value())
		{
			return map_coords::FromWorld(*point);
		}
		return PosOf(target);
	}
	if (registry.AnyOf<components::Abode, components::StoragePit>(target))
	{
		const auto* info = abode_villagers::InfoOf(target);
		const auto type = info != nullptr ? info->abodeType : AbodeType::General;
		switch (type)
		{
		case AbodeType::Creche:
		case AbodeType::Graveyard:
		case AbodeType::Wonder:
		case AbodeType::TownCentre:
			return PosOf(target);
		default:
		{
			// the door
			const auto door = abode_queries::GetArrivePos(target);
			return map_coords::MapCoords {door.x, door.y, 0.0f};
		}
		}
	}
	return PosOf(target); // any other thing: its position
}

uint32_t SetupMoveOnFootpath(entt::entity living, entt::entity footpath, int32_t direction, uint8_t final,
                             footpaths::NodeId node)
{
	auto& fp = FootpathOf(living);
	fp.next = entt::null;
	LeaveNode(living, fp); // the old node
	fp.footpath = footpath;
	fp.reverse = (direction & 1) != 0;
	if (node == footpaths::k_NoNode)
	{
		node = footpaths::GetNearestPos(footpath, PosOf(living), fp.reverse ? 1 : 0);
	}
	fp.node = node;
	if (node == footpaths::k_NoNode)
	{
		return 0;
	}
	footpaths::AddOccupant(footpath, node, living); // at the head
	SetupMobileMoveToPos(living, footpaths::NodeCoords(footpath, node), k_MoveToFirstNode);
	// SetCurrentAndDestinationState(29, final); its result 1 would set the clip, which the state change picks in
	// openblack
	return villager::SetCurrentAndDestinationState(living, VillagerStates::MoveOnPath, static_cast<VillagerStates>(final));
}

uint32_t SetupMoveToWithHug(entt::entity living, const map_coords::MapCoords& pos, uint8_t final)
{
	// (approximate) the goal in metres for the vec2 overload
	return villager::SetupMoveToWithHug(living, glm::vec2(map_coords::ToMetres(pos.x), map_coords::ToMetres(pos.z)),
	                                    static_cast<VillagerStates>(final));
}

void SetNextFootpath(entt::entity living, entt::entity footpath)
{
	FootpathOf(living).next = footpath;
}

bool IsFollowingNode(entt::entity living, entt::entity footpath, footpaths::NodeId node)
{
	auto& registry = Entities();
	const auto* action = registry.TryGet<const components::LivingAction>(living);
	const auto* fp = registry.TryGet<const LivingFootpath>(living);
	return action != nullptr && fp != nullptr &&
	       action->states[static_cast<size_t>(components::LivingAction::Index::Top)] ==
	           static_cast<uint8_t>(VillagerStates::MoveOnPath) &&
	       fp->footpath == footpath && fp->node == node;
}

bool GoesTowardHead(entt::entity living)
{
	const auto* fp = Entities().TryGet<const LivingFootpath>(living);
	return fp != nullptr && fp->reverse;
}

void RePlanHug(entt::entity living)
{
	// openblack's LINEAR move to the goal it has (the goal is not written: no metres -> MapCoords -> metres round
	// trip), with a fresh step and sweep ((approximate) the sweep is PathfindingSystem's LinearScanForObstacle on the
	// next step)
	if (auto* wallHug = Entities().TryGet<components::WallHug>(living); wallHug != nullptr)
	{
		RestartLinearMove(living, *wallHug);
	}
}

void MoveToNode(entt::entity living, entt::entity footpath, footpaths::NodeId node)
{
	auto& fp = FootpathOf(living);
	fp.footpath = footpath;
	fp.node = node;
	footpaths::AddOccupant(footpath, node, living); // at the head
	// the goal = the node's coords, a LINEAR move, a fresh step, the sweep
	SetupMobileMoveToPos(living, footpaths::NodeCoords(footpath, node), k_MoveToFirstNode);
}
} // namespace openblack::ecs::living_footpath
