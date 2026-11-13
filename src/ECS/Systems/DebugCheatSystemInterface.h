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

#include "Common/DebugCheats.h"
#include "Enums.h"

namespace openblack::ecs::systems
{

/// The debug cheats of the players (debug_cheats). The command of a turn turns a cheat on or off, and the rules of the
/// game ask Has for the player. The cheats of a player are on the entity of the player, so they are in the synced state.
class DebugCheatSystemInterface
{
public:
	virtual ~DebugCheatSystemInterface() = default;

	/// Turns a cheat of a player on or off, at the turn of the command. A player that the land does not have gets no
	/// cheat. False when the cheat did not change.
	virtual bool Set(PlayerNames player, debug_cheats::Cheat cheat, bool on) = 0;
	/// The set of the cheats of a player that are on (debug_cheats::BitOf), none for a player with no cheat
	[[nodiscard]] virtual uint32_t CheatsOf(PlayerNames player) const = 0;
	[[nodiscard]] bool Has(PlayerNames player, debug_cheats::Cheat cheat) const
	{
		return debug_cheats::Has(CheatsOf(player), cheat);
	}
	/// Once a turn, after the commands of the turn: the worship sites and the prayer power of each player get the flags
	/// that the cheats of their player give. A site that went to another player gets the flags of that player.
	virtual void ProcessTurn() = 0;
};

} // namespace openblack::ecs::systems
