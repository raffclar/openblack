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

/// The sparkles over the target of a creature's leash order, by the number of their effect on this computer. The world
/// never reads it, and a computer with no particles (the game server) has none.
struct LeashOrderSparkles
{
	uint32_t effect {0};
};

} // namespace openblack::ecs::components
