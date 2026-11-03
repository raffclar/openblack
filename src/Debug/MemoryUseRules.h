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

#include <span>
#include <string>
#include <vector>

/// The rules of the debug window that shows how much memory each kind of component of the world uses
namespace openblack::debug::memory_use
{

/// One storage of the world's registry: the components of one kind
struct Storage
{
	/// The name of the component's type
	std::string name;
	/// How many entities have the component
	std::size_t count {0};
	/// How many entities the storage has room for
	std::size_t capacity {0};
	/// The size of one component, 0 for a component with no data or of a type that the reflection does not know
	std::size_t componentSize {0};
};

/// One line of the window
struct Row
{
	std::string name;
	std::size_t count {0};
	std::size_t bytes {0};
};

/// The bytes that a storage holds for each place it has room for: the entity and the component. The pages that find
/// the place of an entity are not counted.
[[nodiscard]] std::size_t BytesOf(const Storage& storage, std::size_t entitySize);

/// A line for each storage, the largest first. Storages of the same size are in the order of their names.
[[nodiscard]] std::vector<Row> Rows(std::span<const Storage> storages, std::size_t entitySize);

/// The bytes of all the lines
[[nodiscard]] std::size_t TotalBytes(std::span<const Row> rows);

} // namespace openblack::debug::memory_use
