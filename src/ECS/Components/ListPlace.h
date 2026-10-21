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

namespace openblack::ecs::components
{

/// The place of a thing in the game's list of its kind, as for the dances, the forests and the rewards. Each new thing
/// goes to the front of its list, and the turn of the list takes the newest first.
struct ListPlace
{
	/// How many listed things the game made before this one
	uint64_t made {0};
};

} // namespace openblack::ecs::components
