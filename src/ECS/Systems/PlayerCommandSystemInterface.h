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
#include <cstdint>

#include <functional>
#include <span>
#include <vector>

#include "Commands/PlayerCommands.h"
#include "Commands/TurnCommandSource.h"
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
	virtual void RunSent(uint32_t turn) = 0;
	/// Remove the commands that wait, as for a new land
	virtual void Reset() = 0;
	/// In the next turn, the god of this computer's player brings the creature that the player's profile keeps, as a
	/// skirmish starts. A reset of the commands does not change it. False takes back a creature that is due.
	virtual void SetPlayersCreatureDue([[maybe_unused]] bool due) {}

	/// The function that gets the commands of each turn, in the order that the turn does them
	using TurnListener = std::function<void(uint32_t turn, std::span<const player_commands::Command> commands)>;
	/// Tools: give the commands of each turn to a listener, as the turn does them. An empty function stops it.
	virtual void SetTurnListener(TurnListener listener) = 0;
	/// Tools and a server: the commands that another computer did at a turn. The turn does them after the commands that
	/// this computer sent.
	virtual void Receive(uint32_t turn, std::vector<player_commands::Command> commands) = 0;

	/// The player of this computer, whose number goes on the commands that this computer sends. The view of the game picks
	/// it. The world does not use it.
	virtual void SetSender(PlayerNames player) = 0;
	[[nodiscard]] virtual PlayerNames GetSender() const = 0;
	/// The number of commands that wait for the world, in the open block and in the sent blocks
	[[nodiscard]] virtual size_t PendingCount() const = 0;

	/// A network game: the commands of this computer go to the source at once, and each turn does only the commands
	/// that the source gives for it. None gives back the queue of a single player game. The source must live while it
	/// is set.
	virtual void SetSource(player_commands::TurnCommandSourceInterface* source) = 0;
	/// True when the turn can start: always in a single player game, and in a network game when the source has the
	/// commands of the turn
	[[nodiscard]] virtual bool TurnReady(uint32_t turn) const = 0;
	/// True in a network game
	[[nodiscard]] virtual bool HasSource() const = 0;
	/// True in a network game whose computer is behind the turns of the game server (see TurnCommandSourceInterface)
	[[nodiscard]] virtual bool IsBehind([[maybe_unused]] uint32_t turn) const { return false; }
};

} // namespace openblack::ecs::systems
