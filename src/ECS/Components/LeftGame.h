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

/// On a player: the human of this god left the network game, or did not come back in time. The god is defeated and its
/// slot stays closed. Every computer adds it at the same turn, from the command of the game server.
struct LeftGame
{
	/// The turn at which the god left
	uint32_t turn {0};
};

} // namespace openblack::ecs::components
