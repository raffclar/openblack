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

#include "Enums.h"

// A player slot kept by the game, read by the script, the magic objects and an object's player lookup. It is not the
// local player: the player setup (and a game load) write 7 there and nothing else writes it, and the neutral player
// test compares against the player in that slot. So it is the neutral player's slot.

namespace openblack::magic
{
/// Slot 7: the neutral player, the owner of an ownerless object
inline constexpr PlayerNames k_NeutralPlayerSlot = PlayerNames::NEUTRAL;

/// Script player 0 is the player in the neutral slot, n is game player n - 1; none out of 0..7 (there is no game
/// player from 8 on)
[[nodiscard]] inline bool ScriptPlayerToGamePlayer(int32_t script, PlayerNames& player)
{
	const int32_t game = script == 0 ? static_cast<int32_t>(k_NeutralPlayerSlot) : script - 1;
	if (game < 0 || game >= static_cast<int32_t>(PlayerNames::_COUNT))
	{
		return false;
	}
	player = static_cast<PlayerNames>(game);
	return true;
}
} // namespace openblack::magic
