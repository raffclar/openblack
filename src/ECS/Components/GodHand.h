/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Enums.h"

namespace openblack::ecs::components
{

/// On a hand of a god of a network game that is not the first player's: the player whose hand it is. The hands of the
/// first player have none. Every computer makes these hands at the same turn, from the command of the game server.
struct GodHand
{
	PlayerNames player {PlayerNames::PLAYER_TWO};
};

} // namespace openblack::ecs::components
