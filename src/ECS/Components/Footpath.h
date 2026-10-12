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

} // namespace openblack::ecs::components
