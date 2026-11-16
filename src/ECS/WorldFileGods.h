/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <span>

#include "Enums.h"

/// The gods of a world file that the editor loads in a network game: the players of the room keep their gods
namespace openblack::ecs::world_file_gods
{

/// A god of the loaded world file, before the room takes it
struct God
{
	PlayerNames player {PlayerNames::NEUTRAL};
	/// A player of the room has the slot of the god, also a player who is away
	bool roomPlayer {false};
	/// The god has hands in the file. A god with hands is a human of a network game.
	bool hands {false};
	/// A computer mind plays the god in the file
	bool computerMind {false};
};

/// What changes on a god of the file, so that the gods of the world are as the room
struct Change
{
	/// The god gets hands of its own, as at the start of a network game
	bool makeHands {false};
	/// The computer mind of the god switches off, and the player is no longer a computer player
	bool mindOff {false};
	/// The hand of the god lets go, as the hand of a human who leaves, and the hands go. The god is then as a slot with no
	/// player.
	bool removeHands {false};

	[[nodiscard]] bool operator==(const Change&) const = default;
};

/// The change of a god of the file:
/// - The god of a player of the room gets hands when it has none, and a human plays it, not a computer mind.
/// - Another god with hands loses them. The first player always keeps its hands, as at the start of a network game.
/// - Another god without hands stays as it is.
[[nodiscard]] constexpr Change ChangeOf(const God& god) noexcept
{
	if (god.roomPlayer)
	{
		return {.makeHands = !god.hands, .mindOff = god.computerMind, .removeHands = false};
	}
	return {.makeHands = false, .mindOff = false, .removeHands = god.hands && god.player != PlayerNames::PLAYER_ONE};
}

/// After the load of a world file, with the systems of the game: each god of the slots changes as ChangeOf gives. The
/// players are the gods of the players of the room.
void GiveToRoom(std::span<const PlayerNames> players);

} // namespace openblack::ecs::world_file_gods
