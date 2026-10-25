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

#include <optional>
#include <span>
#include <vector>

#include "Commands/PlayerCommands.h"

namespace openblack::player_commands
{

/// The source of the commands of each turn in a network game. A client sends the commands of its player to the game
/// server, and does each turn only with the commands that the server ordered for that turn. The game server orders the
/// commands of all the clients into its turns. A single player game has no source: its commands wait in the queue of
/// the computer.
class TurnCommandSourceInterface
{
public:
	virtual ~TurnCommandSourceInterface() = default;

	/// A command of the player of this computer. It goes to the server at once.
	virtual void Send(const Action& action) = 0;
	/// True when the commands of the turn are here and the turn can start
	[[nodiscard]] virtual bool HasTurn(uint32_t turn) const = 0;
	/// The commands of all the players for the turn, in the order of the server
	[[nodiscard]] virtual std::vector<Command> TakeTurn(uint32_t turn) = 0;
	/// True when this computer is behind the turns of the server: the turn after this one is here too. The server itself,
	/// which closes each turn when its world asks for it, is never behind.
	[[nodiscard]] virtual bool IsBehind(uint32_t turn) const { return HasTurn(turn + 1); }
	/// The creature file that the player of the god sent to all the players before the start. It is empty when the
	/// player has no creature. The server and each client have the same files, so each world makes the same creatures.
	[[nodiscard]] virtual std::span<const uint8_t> StartCreatureFile(PlayerNames /*god*/) const { return {}; }
	/// The god that the player of this computer plays. The server plays no god.
	[[nodiscard]] virtual std::optional<PlayerNames> OwnGod() const { return std::nullopt; }
};

} // namespace openblack::player_commands
