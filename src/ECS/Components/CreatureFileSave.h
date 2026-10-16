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

/// A creature of the player's that is written to the player's creature files as it lives. It keeps the number of game
/// turns the creature had lived when it was last looked at, so that it is written each time that number gets to a
/// multiple of 600.
struct CreatureFileSave
{
	uint32_t turnsSeen {0};
};

} // namespace openblack::ecs::components
