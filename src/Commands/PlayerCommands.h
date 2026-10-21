/******************************************************************************
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

#include <variant>
#include <vector>

#include <entt/entity/entity.hpp>

#include "Creature/LeashKeys.h"
#include "Enums.h"

/// The commands of the players. The game does not change the world at the time of the player's input. The input makes a
/// command, and the command changes the world at the start of a later game turn. Every player, also the one player of a
/// single game, sends the commands this way, so that all the computers of a network game do the same commands at the
/// same turn.
///
/// The steps of a command:
/// 1. The input puts the command in the open block of its player.
/// 2. Once a frame, after the turns of the frame, the game sends the open block. A block that becomes full is sent at
///    once.
/// 3. At the start of each turn, before the hand's own work of the turn, the game does the commands of all the blocks
///    that it sent, in the order that it sent them. Each command has the number of the player that sent it.
///
/// Thus a command never changes the world in the turn that runs in the same frame step as its input, and the world sees
/// it at the start of the next turn after the send.
namespace openblack::player_commands
{

/// A leash shortcut key: the leash key puts on or takes off the leash, and the two other keys change the leash
struct LeashKey
{
	creature_leash::LeashKey key;
};

/// A click on a creature, which puts the leash on the player's creature
struct LeashTap
{
	entt::entity creature;
};

/// The hand let go of a creature that it held: the creature feels how the hand stroked or slapped it
struct CreatureFeedback
{
	entt::entity creature;
	/// The sum of the strokes and the slaps, in the range from -1 to 1
	float amount;
};

/// What a command does
using Action = std::variant<LeashKey, LeashTap, CreatureFeedback>;

/// A command and the player that sent it
struct Command
{
	PlayerNames player;
	Action action;
};

/// The commands that the players made and that the world did not see yet. The queue keeps no other game state, so that
/// the game and the tests use it both.
class CommandQueue
{
public:
	/// The most commands in one block. The block goes at once when it is full.
	static constexpr size_t k_BlockSize = 6;

	/// Put a command in the open block, with the number of the player that sends it. Send the block if it is full.
	void Submit(PlayerNames sender, Action action);
	/// Send the open block, if it has commands. The commands then wait for the start of the next turn.
	void Send();
	/// Take the commands of all the blocks that the queue sent, in the order that it sent them
	[[nodiscard]] std::vector<Command> TakeSent();
	/// Remove all the commands, those that wait and those in the open block
	void Clear();

	/// The number of commands in the open block
	[[nodiscard]] size_t OpenCount() const { return _open.size(); }
	/// The number of commands that the queue sent and that wait for the start of a turn
	[[nodiscard]] size_t SentCount() const;
	/// The number of blocks that the queue sent and that wait for the start of a turn
	[[nodiscard]] size_t SentBlocks() const { return _sent.size(); }

private:
	std::vector<Command> _open;
	std::vector<std::vector<Command>> _sent;
};

} // namespace openblack::player_commands
