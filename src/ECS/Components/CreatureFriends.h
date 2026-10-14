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

namespace openblack::ecs::components
{

/// The creatures that a script made friends of this creature. A creature is never angry with a friend. A script that
/// makes two creatures friends adds each creature to the list of the other, and nothing removes them.
struct CreatureFriends
{
	/// The friends, by their entity numbers
	std::vector<uint32_t> friends;
};

} // namespace openblack::ecs::components
