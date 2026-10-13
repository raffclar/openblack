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
	/// The buffer of these indices, made by make(indices) the first time they are asked for
	template <typename Make>
	[[nodiscard]] const Buffer& Get(std::vector<uint16_t> indices, Make&& make)
	{
		auto found = _lists.find(indices);
		if (found == _lists.end())
		{
			auto buffer = std::forward<Make>(make)(std::span<const uint16_t>(indices));
			found = _lists.emplace(std::move(indices), std::move(buffer)).first;
		}
		return *found->second;
	}

	/// How many different lists have been made
	[[nodiscard]] size_t Size() const { return _lists.size(); }

private:
	std::map<std::vector<uint16_t>, std::unique_ptr<Buffer>> _lists;
};

} // namespace openblack
