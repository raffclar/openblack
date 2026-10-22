/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include "Commands/PlayerCommands.h"
#include "Enums.h"

namespace openblack::ecs::systems
{

/// The queue of the players' commands. The input of a player sends a command. The world does the command at the start of
/// a later turn (see player_commands).
class PlayerCommandSystemInterface
{
public:
	virtual ~PlayerCommandSystemInterface() = default;

	/// Send a command from the player of this computer
	virtual void Submit(player_commands::Action action) = 0;
	/// Send the open block of commands, after the turns of the frame
	virtual void SendOpenBlock() = 0;
	/// Do the commands that wait, at the start of the turn
	virtual void RunSent() = 0;
	/// Remove the commands that wait, as for a new land
	virtual void Reset() = 0;

	/// The player of this computer, whose number goes on the commands that this computer sends. The view of the game picks
	/// it. The world does not use it.
	virtual void SetSender(PlayerNames player) = 0;
	[[nodiscard]] virtual PlayerNames GetSender() const = 0;
	/// The number of commands that wait for the world, in the open block and in the sent blocks
	[[nodiscard]] virtual size_t PendingCount() const = 0;
};

} // namespace openblack::ecs::systems
