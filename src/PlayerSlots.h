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

#include "Enums.h"

/// The number of players. A land of the game has 8 player slots: 7 gods and the neutral player (PlayerNames). A network
/// game of openblack has up to 32 players, and each player is a god with one human (deviation 92). The tables for each
/// player grow to the larger number. A saved table of 8 players still loads into a table of 32 players: the saved
/// players fill the start of the table.
namespace openblack::player_slots
{

/// The player slots of a land of the game, the neutral player included
inline constexpr size_t k_LandSlots = static_cast<size_t>(PlayerNames::_COUNT);
/// The most players in a network game
inline constexpr size_t k_MaxPlayers = 32;

static_assert(k_MaxPlayers >= k_LandSlots, "A network game has at least the player slots of a land");

} // namespace openblack::player_slots
