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

#include "Common/X87.h"
#include "Enums.h"

/// The player that a script names by a number. Pure rules, tested on their own.
namespace openblack::script::script_players
{

/// The player of a script's number. The number 0 names the neutral player, on every computer. A number from 1 names
/// the player one less than it, so 1 names the first player.
[[nodiscard]] constexpr PlayerNames NameOf(int32_t scriptPlayer)
{
	if (scriptPlayer == 0)
	{
		return PlayerNames::NEUTRAL;
	}
	// The subtraction wraps, as the game's does
	return static_cast<PlayerNames>(static_cast<uint32_t>(scriptPlayer) - 1u);
}

/// The player of the number that a script gives as a float. The game first takes the whole number by its float rule
/// (see x87::ToInteger), also for a number out of range.
[[nodiscard]] constexpr PlayerNames NameOfValue(float scriptValue)
{
	return NameOf(x87::ToInteger(scriptValue));
}

} // namespace openblack::script::script_players
