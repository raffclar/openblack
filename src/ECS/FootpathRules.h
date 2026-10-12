/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <span>

#include <glm/vec2.hpp>

/// How villagers walk the land's footpaths, as the game does it. A path is a line of nodes, walked either way. A
/// building keeps the paths that lead from it: a villager going to the building joins one of them near where it stands,
/// walks it node by node and stops on its last node. Places are whole map units (a 65536th of a ten metre cell). Pure.
namespace openblack::ecs::footpath_rules
{

/// How far from a villager a path of the building it goes to may start, in metres
constexpr float k_JoinReach = 40.0f;
/// How near to where the villager is going the far end of a path from its town's storage pit must come, in metres
constexpr float k_StoreRouteReach = 10.0f;
/// How near to a node a villager that lost its place on the path must stand to carry on from it, in metres
constexpr float k_RejoinReach = 0.03f;
/// The nodes on the way are walked to a little to one side: up to this many whole units either way along x and z
/// (0.625 m); the last node of a path is walked to exactly
constexpr int32_t k_Jitter = 0x1000;

using Places = std::span<const glm::ivec2>;

/// A path as the walk sees it
struct Path
{
	Places nodes;
	bool walkable {true};
};

/// Where a villager joins a path: which path, which way along it and the node it walks to first
struct Join
{
	size_t path {0};
	bool towardsStart {false};
	uint32_t node {0};

	bool operator==(const Join&) const = default;
};

/// A walk to a building through the villager's town storage pit: along one of the pit's paths to the pit, then along
/// the building's path that leads from there
struct StoreRoute
{
	/// Joins one of the pit's paths
	Join toStore;
	/// Then this path of the building's
	size_t onward {0};

	bool operator==(const StoreRoute&) const = default;
};

/// The node after one along a path, going one way; none at the path's end
[[nodiscard]] std::optional<uint32_t> NextNode(Places nodes, uint32_t node, bool towardsStart);

/// The node a villager standing somewhere walks to first: the node nearest it, or the one after that when the villager
/// is already nearer to that one than the nearest node is
[[nodiscard]] std::optional<uint32_t> NearestNode(Places nodes, glm::ivec2 from, bool towardsStart);

/// The path a villager joins to go somewhere: of the paths, the one with a node nearest the villager, within the reach
/// (strictly). Each path is walked towards whichever of its ends is nearer the goal.
[[nodiscard]] std::optional<Join> NearestPathTo(std::span<const Path> paths, glm::ivec2 from, glm::ivec2 to, float reach);

/// The same done roughly: each path is walked from the end farther from the goal, and is taken when that end is within
/// the reach of the villager (strictly); the end nearest it wins
[[nodiscard]] std::optional<Join> NearestPathToQuick(std::span<const Path> paths, glm::ivec2 from, glm::ivec2 to, float reach);

/// The walk through the town's storage pit when no path of the building starts near the villager: a building path that
/// runs from near the pit to the goal, and a pit path whose far end is near the villager. None when either is missing.
[[nodiscard]] std::optional<StoreRoute> RouteThroughStore(std::span<const Path> buildingPaths, std::span<const Path> storePaths,
                                                          glm::ivec2 from, glm::ivec2 goal, glm::ivec2 storeArrive);

/// The next node a walker heads for once it reaches a node, and whether that is a node on the way (walked to with a
/// little jitter) rather than the path's last; none when the path has ended
struct Leg
{
	uint32_t node {0};
	bool jitter {false};

	bool operator==(const Leg&) const = default;
};
[[nodiscard]] std::optional<Leg> NextLeg(Places nodes, uint32_t node, bool towardsStart);

/// A walker that lost its node carries on from the node nearest it if it stands on it (within the rejoin reach): the
/// node after that one, none otherwise
[[nodiscard]] std::optional<uint32_t> Rejoin(Places nodes, glm::ivec2 from, bool towardsStart);

/// The path a walker goes on along after the first: walked away from whichever end is nearer it
[[nodiscard]] bool OnwardTowardsStart(Places nodes, glm::ivec2 from);
/// Its first node to walk to: the one after the end it starts from (the end itself when the path has one node), none
/// for a path with no nodes
[[nodiscard]] std::optional<uint32_t> OnwardFirstNode(Places nodes, bool towardsStart);

/// A node on the way, moved by two draws in [0, 0x2000): x first, then z
[[nodiscard]] glm::ivec2 Jittered(glm::ivec2 place, uint32_t drawX, uint32_t drawZ);

} // namespace openblack::ecs::footpath_rules
