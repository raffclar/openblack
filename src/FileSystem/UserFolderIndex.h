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

#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace openblack::filesystem
{

/// The tree of a folder that the user gives to the web build, made from the list of its files that the page sends.
/// The names match without letter case, as on Windows, but each entry keeps the name that it has on the disk.
class UserFolderIndex
{
public:
	struct Node
	{
		/// The name on the disk. The root has no name.
		std::string name;
		/// The number of the file in the list of the page, or no value for a folder
		std::optional<uint32_t> file;
		uint64_t size {0};
		/// The nodes in this folder, by the lower case of their names
		std::map<std::string, uint32_t> children;
	};

	/// The tree of a list with one file on each line: its number, its size in bytes and its path with forward slashes,
	/// divided by tabs. The tree ignores lines that it cannot read and a second path that differs only in letter case.
	[[nodiscard]] static UserFolderIndex Parse(std::string_view list);

	/// All nodes. The first node is the root folder.
	[[nodiscard]] std::span<const Node> Nodes() const { return _nodes; }

	/// The node of a path relative to the root, with any letter case, or no value when the tree does not have it
	[[nodiscard]] std::optional<uint32_t> Find(std::string_view path) const;

private:
	std::vector<Node> _nodes {Node {}};
};

/// The name in lower case. Only the letters A to Z change, as the names of the game's files have only these.
[[nodiscard]] std::string LowerName(std::string_view name);

} // namespace openblack::filesystem
