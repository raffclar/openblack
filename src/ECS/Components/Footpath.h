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

#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureRouteSearch.h"

namespace openblack::ecs::components
{

/// A list-like structure of positions a Living can travel on to get from the first node to the last, and vice-versa
struct Footpath
{
	using Id = int;

	struct Node
	{
		/// Where it is drawn: on the land, raised by its height above it
		glm::vec3 position;
		/// Its place on the map, x and z in whole map units (a 65536th of a ten metre cell), as villagers walk to it
		glm::ivec2 place {0, 0};
		/// Laid by a plan made in play, and may be planned again when something is put down on the path
		bool planned {false};
		/// Under something put down on the path: walkers pass it by
		bool hidden {false};
		/// The place of the thing that a stretch of the path was sent round: walkers pass it by
		bool marker {false};
	};

	std::vector<Node> nodes;
	/// Whether villagers may take it; every path of the lands' footpath files may
	bool walkable {true};
};

/// The footpaths that lead from a building, kept on the building. The position is where the land's footpath file put
/// the link, which found the building standing there.
struct FootpathLink
{
	using Id = int;

	glm::vec3 position;
	std::vector<Footpath::Id> footpaths;
};

/// A villager walking a footpath node by node: the path, which way along it, the node it is heading for, and the path
/// it goes on along once this one ends (from a storage pit on to where it was going)
struct FootpathWalk
{
	entt::entity footpath {entt::null};
	/// Towards the path's first node rather than its last
	bool towardsStart {false};
	/// The node it is walking to, none once the path has ended
	std::optional<uint32_t> node;
	entt::entity then {entt::null};
};

/// A footpath being laid between two things over several game turns: the creatures' route search finds its way a few
/// steps a turn, and the path is laid once the way is found. The path keeps only its two ends until then.
struct FootpathFinder
{
	/// The search for the way of the path and what it goes round, kept from turn to turn until the path is found
	struct Plan
	{
		creature_route::Obstacles obstacles;
		std::optional<creature_route::Search> search;
		glm::vec2 start {0.0f};
		glm::vec2 goal {0.0f};
		/// Whether the obstacles know again how to fill a block and which circles to leave out. A save does not keep
		/// this, so a loaded plan gets it again before its next step.
		bool live {false};

		/// Gives the values that a saved game keeps to a visitor
		template <typename Self, typename Visitor>
		static void VisitSaved(Self& self, Visitor&& visit)
		{
			visit(self.obstacles, self.search, self.start, self.goal);
		}
	};

	entt::entity from {entt::null};
	entt::entity to {entt::null};
	entt::entity footpath {entt::null};
	/// None after the load of a saved game: the search then starts again from the two ends, as the game does
	std::optional<Plan> plan;
};

/// A thing that the footpaths go round, put on the map or removed from it: its radius across the ground in metres and its
/// place in whole map units. On their next turn the footpaths go round it, or stop going round it.
struct FootpathObstacleChange
{
	bool put {true};
	float radius {0.0f};
	glm::ivec2 place {0, 0};
};

} // namespace openblack::ecs::components
