/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FootpathRules.h"

#include <algorithm>
#include <array>
#include <bit>

#include "Common/GUtilsDistance.h"

namespace footpath_rules = openblack::ecs::footpath_rules;
namespace gutils = openblack::gutils;
namespace map_coords = openblack::map_coords;

namespace
{

/// Farther than any node can be
constexpr uint32_t k_NoNodeYetBits = 0x4B189676; // 9999990
const float k_NoNodeYet = std::bit_cast<float>(k_NoNodeYetBits);

map_coords::MapCoords Coords(glm::ivec2 place)
{
	return {.x = place.x, .z = place.y, .altitude = 0.0f};
}

/// The game's distance between two places, in metres, through whole map units
float Distance(glm::ivec2 a, glm::ivec2 b)
{
	return gutils::GetDistanceInMetres(Coords(a), Coords(b));
}

/// The exact square of the distance in metres
float DistanceSq(glm::ivec2 a, glm::ivec2 b)
{
	return gutils::GetMetresDistanceSq(Coords(a), Coords(b));
}

} // namespace

std::optional<uint32_t> footpath_rules::NextNode(Places nodes, uint32_t node, bool towardsStart)
{
	if (towardsStart)
	{
		if (node == 0 || node > nodes.size())
		{
			return std::nullopt;
		}
		return node - 1;
	}
	if (static_cast<size_t>(node) + 1 >= nodes.size())
	{
		return std::nullopt;
	}
	return node + 1;
}

std::optional<uint32_t> footpath_rules::NearestNode(Places nodes, glm::ivec2 from, bool towardsStart)
{
	std::optional<uint32_t> nearest;
	float best = k_NoNodeYet;
	for (uint32_t i = 0; i < nodes.size(); ++i)
	{
		const float distance = Distance(nodes[i], from);
		if (distance < best)
		{
			best = distance;
			nearest = i;
		}
	}
	if (!nearest.has_value())
	{
		return std::nullopt;
	}
	// A villager already past the nearest node, nearer the next one than that node is, goes straight on to the next
	if (const auto next = NextNode(nodes, *nearest, towardsStart); next.has_value())
	{
		const float villagerToNext = Distance(nodes[*next], from);
		const float nearestToNext = Distance(nodes[*next], nodes[*nearest]);
		if (nearestToNext > villagerToNext)
		{
			return next;
		}
	}
	return nearest;
}

std::optional<footpath_rules::Join> footpath_rules::NearestPathTo(std::span<const Path> paths, glm::ivec2 from, glm::ivec2 to,
                                                                  float reach)
{
	std::optional<Join> best;
	for (size_t i = 0; i < paths.size(); ++i)
	{
		const auto& path = paths[i];
		if (!path.walkable || path.nodes.empty())
		{
			continue;
		}
		const float firstToGoal = Distance(path.nodes.front(), to);
		const float lastToGoal = Distance(path.nodes.back(), to);
		const bool towardsStart = lastToGoal > firstToGoal;
		const auto node = NearestNode(path.nodes, from, towardsStart);
		if (!node.has_value())
		{
			continue;
		}
		const float distance = Distance(path.nodes[*node], from);
		if (distance < reach)
		{
			reach = distance;
			best = Join {.path = i, .towardsStart = towardsStart, .node = *node};
		}
	}
	return best;
}

std::optional<footpath_rules::Join> footpath_rules::NearestPathToQuick(std::span<const Path> paths, glm::ivec2 from,
                                                                       glm::ivec2 to, float reach)
{
	std::optional<Join> best;
	for (size_t i = 0; i < paths.size(); ++i)
	{
		const auto& path = paths[i];
		if (!path.walkable || path.nodes.empty())
		{
			continue;
		}
		const float firstToGoal = DistanceSq(path.nodes.front(), to);
		const float lastToGoal = DistanceSq(path.nodes.back(), to);
		const bool towardsStart = lastToGoal > firstToGoal;
		const auto start = towardsStart ? static_cast<uint32_t>(path.nodes.size() - 1) : 0u;
		const float distance = Distance(path.nodes[start], from);
		if (distance < reach)
		{
			reach = distance;
			best = Join {.path = i, .towardsStart = towardsStart, .node = start};
		}
	}
	return best;
}

std::optional<footpath_rules::StoreRoute> footpath_rules::RouteThroughStore(std::span<const Path> buildingPaths,
                                                                            std::span<const Path> storePaths, glm::ivec2 from,
                                                                            glm::ivec2 goal, glm::ivec2 storeArrive)
{
	const auto onward = NearestPathToQuick(buildingPaths, goal, storeArrive, k_StoreRouteReach);
	if (!onward.has_value() || storePaths.empty())
	{
		return std::nullopt;
	}
	const auto toStore = NearestPathToQuick(storePaths, from, storeArrive, k_JoinReach);
	if (!toStore.has_value())
	{
		return std::nullopt;
	}
	return StoreRoute {.toStore = *toStore, .onward = onward->path};
}

std::optional<footpath_rules::Leg> footpath_rules::NextLeg(Places nodes, uint32_t node, bool towardsStart)
{
	const auto next = NextNode(nodes, node, towardsStart);
	if (!next.has_value())
	{
		return std::nullopt;
	}
	return Leg {.node = *next, .jitter = NextNode(nodes, *next, towardsStart).has_value()};
}

std::optional<uint32_t> footpath_rules::Rejoin(Places nodes, glm::ivec2 from, bool towardsStart)
{
	const auto nearest = NearestNode(nodes, from, towardsStart);
	if (!nearest.has_value() || Distance(nodes[*nearest], from) > k_RejoinReach)
	{
		return std::nullopt;
	}
	return NextNode(nodes, *nearest, towardsStart);
}

bool footpath_rules::OnwardTowardsStart(Places nodes, glm::ivec2 from)
{
	if (nodes.empty())
	{
		return false;
	}
	return DistanceSq(nodes.front(), from) > DistanceSq(nodes.back(), from);
}

std::optional<uint32_t> footpath_rules::OnwardFirstNode(Places nodes, bool towardsStart)
{
	if (nodes.empty())
	{
		return std::nullopt;
	}
	const auto end = towardsStart ? static_cast<uint32_t>(nodes.size() - 1) : 0u;
	return NextNode(nodes, end, towardsStart).value_or(end);
}

glm::ivec2 footpath_rules::Jittered(glm::ivec2 place, uint32_t drawX, uint32_t drawZ)
{
	const auto offset = [](uint32_t draw) { return static_cast<int32_t>(draw) - k_Jitter; };
	return {static_cast<int32_t>(static_cast<uint32_t>(place.x) + static_cast<uint32_t>(offset(drawX))),
	        static_cast<int32_t>(static_cast<uint32_t>(place.y) + static_cast<uint32_t>(offset(drawZ)))};
}

namespace
{
/// The commands of a land's script that make a thing which a footpath link command after them goes to
constexpr std::array<std::string_view, 32> k_CommandsThatMakeThings {
    "CREATE_TOWN",           "CREATE_MIST",         "CREATE_ABODE",           "CREATE_PLANNED_ABODE", "CREATE_PLANNED_CITADEL",
    "CREATE_CREATURE_PEN",   "CREATE_WORSHIP_SITE", "CREATE_FOREST",          "CREATE_TREE",          "CREATE_NEW_TREE",
    "CREATE_ANIMAL",         "CREATE_FIELD",        "CREATE_DRINK_WAYPOINT",  "CREATE_FISH_FARM",     "CREATE_FEATURE",
    "CREATE_FLOWERS",        "CREATE_NEW_FEATURE",  "CREATE_ANIMATED_STATIC", "CREATE_POT",           "CREATE_MOBILEOBJECT",
    "CREATE_SCAFFOLD",       "CREATE_MOBILESTATIC", "CREATE_MOBILE_STATIC",   "CREATE_DEAD_TREE",     "CREATE_CREATURE",
    "CREATE_INFLUENCE_RING", "CREATE_WATERFALL",    "CREATE_ARENA",           "CREATE_BONFIRE",       "CREATE_BASE",
    "CREATE_STREET_LANTERN", "CREATE_STREET_LIGHT",
};
} // namespace

bool footpath_rules::ScriptCommandMakesThing(std::string_view command)
{
	return std::ranges::find(k_CommandsThatMakeThings, command) != k_CommandsThatMakeThings.end();
}

std::optional<size_t> footpath_rules::ScriptFootpathIndex(size_t count, int32_t number)
{
	if (number < 0 || static_cast<size_t>(number) >= count)
	{
		return std::nullopt;
	}
	return static_cast<size_t>(number);
}
