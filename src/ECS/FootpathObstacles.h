/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "ECS/FootpathPlanning.h"

/// How the game sends a footpath round a thing that is put on it, and back again when the thing is removed. A
/// thing that the creatures' route rule makes them go round is put on the map: each path's nodes under it are hidden and
/// a stretch of the path is planned again round it. The thing is removed from the map: the hidden nodes show again and the
/// stretches planned round it are planned again, or cut out where they only bend round the place it stood.
///
/// The nodes are those of footpath_planning. A node that is not planned, hidden or a marker is an anchor: the planning
/// lays new nodes only between two anchors. Places are whole map units; the maths is in floats, in the game's order. Pure:
/// the route planner, the walkers and the land's height come through the hooks.
namespace openblack::ecs::footpath_obstacles
{

/// A thing put on the map or removed from it: how far it reaches across the ground, in metres, and where it stands
struct Obstacle
{
	float radius {0.0f};
	glm::ivec2 place {0, 0};
};

/// A node of a path, with a key that tells the walkers' node apart from the others while the nodes change. Nodes that
/// the rules make get keys after the largest key that they are given.
struct KeyedNode
{
	footpath_planning::PlanNode node;
	uint32_t key {0};

	bool operator==(const KeyedNode&) const = default;
};

/// What the rules ask of the world
struct Hooks
{
	/// The places of the nodes that the route planner lays between two places at once, none when it finds no way
	std::function<std::optional<std::vector<glm::ivec2>>(glm::ivec2 from, glm::ivec2 to)> planRoute;
	/// The walkers on the path that walk to a node (by its key) one way look again at what is in their way, from where
	/// they stand, going straight on
	std::function<void(uint32_t key, bool towardsStart)> lookAgain;
	/// A node goes: the walkers that walk to it towards the path's start walk to the anchor before it instead, and the
	/// others to the anchor after it (each anchor by its key and place)
	std::function<void(uint32_t gone, uint32_t back, glm::ivec2 backPlace, uint32_t on, glm::ivec2 onPlace)> sendOn;
	/// A node is removed from the path with no word to its walkers: they walk on to it, then those going away from the
	/// path's start continue along the path after a node (by its key), and those going towards the start find that the
	/// path has ended (as at the first node, by its key)
	std::function<void(uint32_t gone, uint32_t onwardFrom, uint32_t first)> cutOff;
	/// The height of the land at a place, in metres
	std::function<float(glm::ivec2 place)> altitude;
};

/// The radius that a path's nodes must be in, squared and widened by a hundredth, in square map units: a node is
/// under the thing when its square distance is less than this
[[nodiscard]] float HideReachSq(float radius);

/// The reach in metres from the place of a thing removed within which its path's nodes are changed back
[[nodiscard]] float ReturnReach(float radius);

/// Whether the angle at the third place between the first two is small enough to cut out a bend: the cross product of
/// the two sides across the ground, over their dot product in three dimensions with the land's height, is less than a
/// hundredth
[[nodiscard]] bool StraightEnough(glm::vec3 first, glm::vec3 second, glm::vec3 corner);

/// Whether a thing put on the map cannot touch a path whose node places all lie in the square from low to high. Then
/// GoRound changes no node and asks nothing of the hooks. A square about the thing that wraps round the map's integers
/// always counts as a touch.
[[nodiscard]] bool GoesClearOf(glm::ivec2 low, glm::ivec2 high, const Obstacle& obstacle);

/// A thing is put on the map: the path is sent round it. Changes the nodes.
void GoRound(std::vector<KeyedNode>& nodes, const Obstacle& obstacle, const Hooks& hooks);

/// A thing is removed: the path stops going round it. Changes the nodes.
void StopGoingRound(std::vector<KeyedNode>& nodes, const Obstacle& obstacle, const Hooks& hooks);

} // namespace openblack::ecs::footpath_obstacles
