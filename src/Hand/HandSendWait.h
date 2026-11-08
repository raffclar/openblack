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

#include "ECS/Components/HandSendWait.h"

/// The wait of the player's hand for its last send. These commands start the wait: the let go of the thing in the hand,
/// the give to a creature, the use of the held thing or seed on a thing or on the land, and a press on the ground of
/// the player's fight. Until the turn does the command, the hand sends no other of these commands. The fight also
/// refuses a press on a body.
///
/// The wait keeps the game turn of the send. The turn that does a command of the player keeps its own number. The hand's
/// pass of the next turn clears both numbers when that turn is not before the send.
namespace openblack::hand_send_wait
{

/// The hand sent a command at a game turn. A send at turn zero does not start a wait (vanilla-bugs row 1255, kept).
constexpr void MarkSent(ecs::components::HandSendWait& wait, uint32_t turn)
{
	wait.sentTurn = turn;
}

/// A turn did a command of the player that the wait covers
constexpr void MarkDone(ecs::components::HandSendWait& wait, uint32_t turn)
{
	wait.doneTurn = turn;
}

/// The hand's pass of a turn, after the commands of the turn. The wait ends when the last command that the turn did is
/// not before the send. Without a wait, the pass also clears the turn of the last command.
constexpr void TurnPass(ecs::components::HandSendWait& wait)
{
	if (wait.sentTurn <= wait.doneTurn)
	{
		wait = {};
	}
}

/// The hand waits for its last send
[[nodiscard]] constexpr bool Waits(const ecs::components::HandSendWait& wait)
{
	return wait.sentTurn != 0;
}

} // namespace openblack::hand_send_wait
