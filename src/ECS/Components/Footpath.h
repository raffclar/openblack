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

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"

namespace openblack::ecs::components
{

/// A list-like structure of positions a Living can travel on to get from the first node to the last, and vice-versa.
/// Its node list, kept here in the list's order (index 0 = the head, the newest node: a new position goes at the
/// head), and `active`. The queries are
/// ecs::footpaths
struct Footpath
{
	using Id = int;

	/// A node: its coords and its a / b flags
	struct Node
	{
		glm::vec3 position; ///< (openblack) the world point, for the debug draw
		map_coords::MapCoords coords;
		uint8_t flags {0}; ///< bit 0 a, bit 1 b, bits 2 and 3 hidden (footpaths::k_HiddenMask)
		/// (openblack) footpaths::NodeId: the node's creation order in its footpath (nextNodeId), never reused, the
		/// original's node pointer a Living keeps
		int32_t id {-1};
		/// the Livings on it, the list's head first (footpaths::AddOccupant)
		std::vector<entt::entity> occupants;
	};

	std::vector<Node> nodes;
	bool active {true}; ///< (true when made; the links' queries skip a footpath without it)
	/// (openblack) a new footpath goes at the head of the game's footpath list: the list's order
	/// is this stamp, the highest first (footpaths::Create)
	uint32_t creationStamp {0};
	int32_t nextNodeId {0}; ///< (openblack) the next Node::id
};

/// The footpath link a thing holds (a MultiMapFixed, a forest or a dance; footpaths::GetFootpathLink). Set by the
/// .fot's links (footpaths::AttachLoadedLink)
struct FootpathLinkOf
{
	entt::entity link {entt::null};
};

/// Links a [Planned]MultiMapFixed entity to a list of footpaths
/// The position is used to look-up matches. If the MMF is close enough to the link, they are connected.
/// The relationship of MMF to FPL is one2one and MMF to Footpath is one2many
struct FootpathLink
{
	using Id = int;

	glm::vec3 position;
	std::vector<Footpath::Id> footpaths;
};

} // namespace openblack::ecs::components
