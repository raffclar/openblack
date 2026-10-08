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

#include <optional>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"

// The footpaths' queries. A footpath is a components::Footpath entity. A node is named by its NodeId
// (Footpath::Node::id, its creation order in the footpath, never reused): the node a Living keeps stays valid while
// its footpath lives (the original never unlinks a single node). How a Living walks them is living_footpath's
// (SetupMoveOnFootpath)

namespace openblack::route_planner
{
class ObstacleGrid;
class RoutePlan;
} // namespace openblack::route_planner

namespace openblack::ecs::footpaths
{
using NodeId = int32_t;
inline constexpr NodeId k_NoNode = -1;

/// Flag bits 2 and 3: a hidden node, skipped by every query
inline constexpr uint8_t k_HiddenMask = 0xC;

/// NextNodeNear / StepNextNode's "no next node" result
inline constexpr int32_t k_EndOfFootpath = 0x2D;

/// The nearest non-hidden node (distance in metres < 1e7, strict, from the head), then the next one in `direction`:
/// that one when it is farther from the nearest than from `from`. k_NoNode for none
[[nodiscard]] NodeId GetNearestPos(entt::entity footpath, const map_coords::MapCoords& from, int32_t direction);
/// direction != 0 the node before it in the list (none before the head), direction == 0 the one after; a hidden one is
/// stepped over in the same direction
[[nodiscard]] NodeId GetNextNode(entt::entity footpath, NodeId node, int32_t direction);
/// direction != 0 the first non-hidden node from the head, else the last; none when every node is hidden
[[nodiscard]] std::optional<map_coords::MapCoords> GetEndNonHiddenNode(entt::entity footpath, int32_t direction);
/// The node's MapCoords; {} for no such node
[[nodiscard]] map_coords::MapCoords NodeCoords(entt::entity footpath, NodeId node);
/// The nearest non-hidden node below `best` (strict), `best` updated; k_NoNode for none
[[nodiscard]] NodeId NearestNodeBelow(entt::entity footpath, const map_coords::MapCoords& from, float& best);
/// n = GetNearestPos(pos, direction); none or n farther than maxDistance -> 0; no next node after n ->
/// k_EndOfFootpath; else node = the next one, out = its coords, 1
[[nodiscard]] int32_t NextNodeNear(entt::entity footpath, const map_coords::MapCoords& pos, NodeId& node,
                                   map_coords::MapCoords& out, int32_t direction, float maxDistance);
/// GetNextNode(node, direction) none -> k_EndOfFootpath; else node = it, out = its coords, 1
[[nodiscard]] int32_t StepNextNode(entt::entity footpath, NodeId& node, map_coords::MapCoords& out, int32_t direction);

/// The list's ends, hidden or not: the head (the newest node) and the last one; k_NoNode when empty
[[nodiscard]] NodeId HeadNode(entt::entity footpath);
[[nodiscard]] NodeId TailNode(entt::entity footpath);

/// The Livings on the node, the list's head first (SetupMoveOnFootpath links one at the head and unlinks it from its
/// old node). Nothing for no such node
void AddOccupant(entt::entity footpath, NodeId node, entt::entity living);
void RemoveOccupant(entt::entity footpath, NodeId node, entt::entity living);

/// The footpath, its node and its direction a link's query chose
struct PathChoice
{
	entt::entity footpath {entt::null};
	NodeId node {k_NoNode};
	int32_t direction {0};
};
/// For each active footpath of the link (from its list head) direction = (its tail farther from `to` than its head) ?
/// 1 : 0, GetNearestPos(from, direction); a node nearer `from` than `best` (strict) is kept and `best` updated. The
/// last kept, or none
[[nodiscard]] std::optional<PathChoice> GetNearestPathTo(entt::entity link, const map_coords::MapCoords& from,
                                                         const map_coords::MapCoords& to, float& best);
/// The same with the ends' squared distances to `to` and the far end as the entry node, no GetNearestPos
[[nodiscard]] std::optional<PathChoice> GetNearestPathToQuick(entt::entity link, const map_coords::MapCoords& from,
                                                              const map_coords::MapCoords& to, float& best);

/// The thing's footpath link (components::FootpathLinkOf): multi-map fixed objects, forests and dances have one.
/// entt::null for none
[[nodiscard]] entt::entity GetFootpathLink(entt::entity thing);
/// A link loaded from the .fot: it goes to the first object of its MapCoords' cell (from the head) within a squared
/// distance of 0.0025 that is a multi-map fixed object; else to the first planned abode within it, the towns newest
/// first and each town's plans oldest first; the old link of that slot is deleted. None: the link is deleted
void AttachLoadedLink(entt::entity link, const map_coords::MapCoords& coords);

/// A new footpath between a's and b's arrive positions (the head at b's), planned round the obstacles (each end first
/// pushed out of its own multi-map fixed object), then given to both (AddFootpath). entt::null when the planner finds
/// no way (the footpath deleted)
entt::entity GenerateRoutesBetween(entt::entity a, entt::entity b);
/// An abode made functional: a new footpath from the abode's arrive position to the town's storage pit's (the head at
/// the pit's), AttemptRerender(head, tail, none); found -> AddFootpath on the abode, then on the pit; else the footpath
/// deleted. entt::null when none was made
entt::entity MakeAbodeFootpath(entt::entity abode, entt::entity pit);
/// The stretch between two nodes of a footpath planned again; true when found (the nodes between replaced,
/// ConvertCreaturePlanToFootpath)
bool AttemptRerender(entt::entity footpath, NodeId a, NodeId b, const map_coords::MapCoords* c);
/// Every footpath round a new obstacle (a fixed object placed on the map): the nodes inside it hidden (a visible copy
/// after each), then the first stretch that meets it re-planned (AttemptRerender, a hidden node at pos) or bent round
/// it in 18-degree steps
void RerouteFootpathsAroundObstacle(float r, const map_coords::MapCoords& pos);
/// Its undo, when the obstacle leaves the map, within 1.03 r + 0.4
void StopReroutingAroundObstacle(float r, const map_coords::MapCoords& pos);
/// The plan's best route as nodes between a and b (the arcs in 18-degree steps), the old nodes between them deleted
/// but the hidden ones kept after the new ones; with c, one more hidden node at c
void ConvertCreaturePlanToFootpath(const route_planner::ObstacleGrid& holder, const route_planner::RoutePlan& plan,
                                   entt::entity footpath, NodeId a, NodeId b, const map_coords::MapCoords* c);
/// The thing's link, made on first use, gets the footpath at the head of its list
void AddFootpath(entt::entity thing, entt::entity footpath);
/// (openblack) the "footpaths" part of the turn state hash (Debug/StateHash.h): every footpath's nodes (coords, flags)
/// in list order, the footpaths newest first. Registered on every land load
void RegisterStateHash();

/// (1) the link's footpath nearest the Living (GetNearestPathTo(its position, pos, 40)) and SetupMoveOnFootpath on it
/// != 0 -> 1; (2) else, through the town's storage pit (owner's town, or pos's nearest; a pit that is not the owner):
/// this link's footpath from pos toward the pit (GetNearestPathToQuick(pos, pit, 10)) and the pit link's from the
/// Living toward the pit (Quick(living, pit, 40)) -> SetupMoveOnFootpath on the second, the first as the next
/// footpath, 1; (3) else SetupMoveToWithHug
[[nodiscard]] uint32_t UseFootpathIfNecessary(entt::entity link, entt::entity living, const map_coords::MapCoords& pos,
                                              uint8_t final, entt::entity owner);

/// A new footpath (active, no node) pushed at the head of the footpath list. Every maker of footpaths goes through it:
/// the map script's CREATE_FOOTPATH, the .fot load, the planner's routes
[[nodiscard]] entt::entity Create();
/// The footpath's position in the list from the head (0 = the newest), -1 if absent. (openblack) only the tests call
/// it today: the original's other callers are the footpath / link saves (not ported)
[[nodiscard]] int32_t PositionFromHead(entt::entity footpath);
/// The footpath at `position` from the head (the inverse of PositionFromHead), entt::null if none
[[nodiscard]] entt::entity AtPositionFromHead(int32_t position);
} // namespace openblack::ecs::footpaths
