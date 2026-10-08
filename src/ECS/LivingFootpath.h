/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "ECS/Footpaths.h"

// How a Living walks a footpath, the Living side. The footpaths, their nodes and the nodes' occupants are
// ECS/Footpaths.h's; its UseFootpathIfNecessary and the reroute of RerouteFootpathsAroundObstacle call the functions
// below. Still to come: the move to a footpath, the MOVE_ON_PATH state itself and its exit

namespace openblack::ecs::components
{
/// The Living's footpath fields
struct LivingFootpath
{
	entt::entity next {entt::null}; ///< The footpath to take after this one (UseFootpathIfNecessary's)
	entt::entity footpath {entt::null};
	footpaths::NodeId node {footpaths::k_NoNode};
	bool reverse {false}; ///< The direction along the footpath (towards the head)
};
} // namespace openblack::ecs::components

namespace openblack::ecs::living_footpath
{
/// The move-to states the walk on a footpath gives SetupMobileMoveToPos: 12 on the first node and in the reroute, 11
/// between nodes
inline constexpr uint8_t k_MoveToFirstNode = 0xC;
inline constexpr uint8_t k_MoveToNextNode = 0xB;

/// The object's position as MapCoords ((approximate) from the Transform; altitude 0)
[[nodiscard]] map_coords::MapCoords PosOf(entt::entity object);
/// The thing's arrive position by its class (`living` only for the BigForest):
/// - the door: an abode, a storage pit and every other multi-map fixed object (abode_queries::GetArrivePos);
/// - the position: a creche, a graveyard, a wonder, a town centre, a field, a fish farm and any other thing;
/// - a worship site: its special point 9 when it has it, else its position;
/// - BigForest: object::BigForestGetArrivePos(forest, living);
/// - (pending) a dance: its position with x + 0x8000: openblack has no Dance entity
[[nodiscard]] map_coords::MapCoords ArrivePosOf(entt::entity target, entt::entity living = entt::null);

/// No next footpath, the footpath set; out of the old node's occupants; the direction = direction & 1; no node ->
/// footpaths::GetNearestPos(this, direction); none -> 0; into the node's occupants (head); SetupMobileMoveToPos(node,
/// 12); SetCurrentAndDestinationState(29 MOVE_ON_PATH, final), which picks the clip. Returns that
uint32_t SetupMoveOnFootpath(entt::entity living, entt::entity footpath, int32_t direction, uint8_t final,
                             footpaths::NodeId node);
/// SetupMoveToWithHug with a MapCoords goal (UseFootpathIfNecessary's fallback)
uint32_t SetupMoveToWithHug(entt::entity living, const map_coords::MapCoords& pos, uint8_t final);
/// The footpath to take after this one (UseFootpathIfNecessary's path through the storage pit)
void SetNextFootpath(entt::entity living, entt::entity footpath);

// ---- for the footpath's rebuild and its reroute around an obstacle (PurgeFollowerList, ClearFromPreviousNode /
// ClearFromNextNode, ClearForDeletion, RerouteFootpathsAroundObstacle) -------------------------------------------------
/// PurgeFollowerList's test: the living is in state 29 MOVE_ON_PATH on `node` of `footpath`
[[nodiscard]] bool IsFollowingNode(entt::entity living, entt::entity footpath, footpaths::NodeId node);
/// It walks the footpath towards the list's head (footpaths::GetNextNode's direction != 0)
[[nodiscard]] bool GoesTowardHead(entt::entity living);
/// A LINEAR move round obstacles from where it stands: the walk re-planned towards the goal it has. The footpath, the
/// node and state 29 are KEPT. The reroute calls it: ClearFromNextNode on the previous node for the followers with
/// GoesTowardHead, ClearFromPreviousNode on the current one for those without
void RePlanHug(entt::entity living);
/// ClearForDeletion's move: the node set, at the head of its occupants (footpaths::AddOccupant), the goal = its
/// coords, a fresh LINEAR move round obstacles
void MoveToNode(entt::entity living, entt::entity footpath, footpaths::NodeId node);
} // namespace openblack::ecs::living_footpath
