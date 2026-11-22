/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// The speed of the game, as the speed keys, the scripts and the playback of the hand set it
namespace openblack::ecs::game_speed
{

/// Sets the speed of the game: 1 is normal and 2 twice as fast. A game alone sets its clock to the speed. A network game
/// keeps the speed on this computer and asks the game server for the time between two turns of every player; the clock
/// of each computer then follows the clock of the room. The world of the game server changes no clock for it.
void Set(float speed);

/// The speed of the game that the speed keys and the words of the speed read: the speed of the clock in a game alone,
/// and the speed that this computer set last in a network game
[[nodiscard]] float Get();

} // namespace openblack::ecs::game_speed
