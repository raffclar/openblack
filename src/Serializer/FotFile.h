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

#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>

namespace openblack
{

class FotFile
{
public:
	void Load(const std::filesystem::path& path);
};

/// The parts of a land's footpath file, as the game writes them, with no game state
namespace fot_file
{

/// A node of a footpath
struct Node
{
	/// Its place on the map, in whole map units
	glm::ivec2 place {0, 0};
	/// Its height above the land
	float altitude {0.0f};
	/// Its flags: 1 for each node that the game makes from a land script or a plan, 2 for a node that a plan laid, 4 for
	/// a node under a thing, 8 for the place of a thing that the path goes round
	uint8_t flags {1};
};

/// A footpath: its nodes in order, and whether villagers may take it
struct Path
{
	std::vector<Node> nodes;
	bool walkable {true};
};

/// The footpaths of one thing at its place. The paths are the numbers of the paths in the list of the file. A thing with
/// no link keeps no paths.
struct LinkSave
{
	glm::ivec2 place {0, 0};
	float altitude {0.0f};
	std::optional<std::vector<size_t>> paths;
};

/// The bytes of a land's footpath file: the link of each thing, then each footpath. Each record starts with its number,
/// its kind, its player (always 0), the check sum of the bytes before it, and two zero fields. The record of a footpath
/// goes in the file at the first link that names it, and later links give only its number. A link to a number that is
/// not in the list of paths is not written.
[[nodiscard]] std::vector<uint8_t> Write(std::span<const LinkSave> links, std::span<const Path> paths);

} // namespace fot_file

} // namespace openblack
