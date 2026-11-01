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
#include <string>
#include <vector>

#include "CreatureRecord.h"
#include "ECS/Components/Footpath.h"

namespace openblack::gamesave
{
class VanillaWorld;
} // namespace openblack::gamesave

// The footpaths of a world file of the game, the links that join them to things and the walks of the villagers on them,
// as values that openblack can make again. The rules here use no service of the game, so that tests can give them a
// world that they make.
namespace openblack::ecs::vanilla_footpath_import
{

/// The place of the game's list of all the footpaths among the lists of the game object
inline constexpr size_t k_FootpathList = 28;

/// The flags of a node in the file. The game sets the first flag on each node that it makes.
inline constexpr uint8_t k_NodeMade = 0x01;
inline constexpr uint8_t k_NodePlanned = 0x02;
inline constexpr uint8_t k_NodeHidden = 0x04;
inline constexpr uint8_t k_NodeMarker = 0x08;
/// The flags that openblack keeps on a node
inline constexpr uint8_t k_NodeKeptFlags = k_NodeMade | k_NodePlanned | k_NodeHidden | k_NodeMarker;

/// A node of a footpath in the file
struct ImportedNode
{
	uint32_t number {0};
	gamesave::MapCoords coords {};
	uint8_t flags {0};
	/// The numbers of the living things that walk to the node, in the order of the file
	std::vector<uint32_t> followers;
};

/// A footpath in the file
struct ImportedFootpath
{
	uint32_t number {0};
	/// Its nodes from the first to the last
	std::vector<ImportedNode> nodes;
	/// Not 0 when walkers can take the footpath
	uint32_t walkable {0};
};

/// The link of a thing of the file to its footpaths
struct ImportedLink
{
	/// The numbers of the thing and of its link in the file
	uint32_t holder {0};
	uint32_t number {0};
	/// The numbers of its footpaths in the order of the file
	std::vector<uint32_t> footpaths;
};

/// The footpaths of a file
struct ImportedFootpaths
{
	/// The footpaths in the order of the game's list of the footpaths: the newest first
	std::vector<ImportedFootpath> paths;
	/// The links of the things, in the order of the records of the things
	std::vector<ImportedLink> links;
	/// The number of searches for a way of a footpath under way
	uint32_t finders {0};
	/// What the import does not take, and why
	std::vector<std::string> skipped;
};

/// The footpaths, the links and the searches of the file
[[nodiscard]] ImportedFootpaths ReadFootpaths(const gamesave::VanillaWorld& world);

/// A node of openblack with the place and the flags of a node of the file. Its position is on the land at the place,
/// raised by the height of the node, with the height of the land given.
[[nodiscard]] components::Footpath::Node NodeOf(const ImportedNode& node, float landHeight);

/// A footpath walk of a villager of the file, as the numbers of the file
struct FileWalk
{
	/// The footpath that it walks and the node that it walks to, 0 for none
	uint32_t footpath {0};
	uint32_t node {0};
	/// The walk goes towards the first node of the footpath
	bool towardsStart {false};
	/// The footpath that it goes on along when this footpath ends, 0 for none
	uint32_t next {0};
};

/// The place of the node in the footpath: none when the walk has no node, or when the node is not one of the footpath
[[nodiscard]] std::optional<uint32_t> NodeIndexOf(const ImportedFootpath& footpath, uint32_t node);

} // namespace openblack::ecs::vanilla_footpath_import
