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
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace openblack
{

/// Index lists made into buffers once and shared by everything that draws the same list. Most of a land's blocks (the
/// open sea, plain land) have cells of the same flags and so draw the same triangles: sharing their buffers keeps an
/// island to a few hundred of the renderer's index buffers rather than one or more for each of its thousand blocks.
template <typename Buffer>
class SharedIndexLists
{
public:
	/// The buffer of these indices. The first request makes it with make(indices). An empty list has no buffer and
	/// gives nullptr: a block of open sea only has no triangles, and the graphics library cannot make an empty buffer.
	template <typename Make>
	[[nodiscard]] const Buffer* Get(std::vector<uint16_t> indices, Make&& make)
	{
		if (indices.empty())
		{
			return nullptr;
		}
		auto found = _lists.find(indices);
		if (found == _lists.end())
		{
			auto buffer = std::forward<Make>(make)(std::span<const uint16_t>(indices));
			found = _lists.emplace(std::move(indices), std::move(buffer)).first;
		}
		return found->second.get();
	}

	/// How many different lists have been made
	[[nodiscard]] size_t Size() const { return _lists.size(); }

	/// Calls visit(indices, buffer) for each list, in the order of the lists
	template <typename Visit>
	void ForEach(Visit&& visit) const
	{
		for (const auto& [indices, buffer] : _lists)
		{
			visit(std::span<const uint16_t>(indices), *buffer);
		}
	}

private:
	std::map<std::vector<uint16_t>, std::unique_ptr<Buffer>> _lists;
};

} // namespace openblack
