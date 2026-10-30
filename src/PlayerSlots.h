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

#include <optional>

#include "Enums.h"

/// The number of players. A land of the game has 8 player slots: 7 gods and the neutral player (PlayerNames). A network
/// game of openblack has up to 32 players, and each player is a god with one human (deviation 92). The tables for each
/// player grow to the larger number. A saved table of 8 players still loads into a larger table: the saved players fill
/// the start of the table.
///
/// The neutral player keeps its number 7 on every land, as the lands and the scripts of the game name it. The first 7
/// gods of a network game have the numbers of the gods of a land (0 to 6). The gods after them take the numbers after the
/// neutral player (8 to 32). Thus every land of the game and every game alone keep their player numbers.
namespace openblack::player_slots
{

/// The player slots of a land of the game, the neutral player included
inline constexpr size_t k_LandSlots = static_cast<size_t>(PlayerNames::_COUNT);
/// The gods of a land of the game: each player slot before the neutral player
inline constexpr size_t k_LandGods = static_cast<size_t>(PlayerNames::NEUTRAL);
/// The most players in a network game
inline constexpr size_t k_MaxPlayers = 32;
/// The player numbers of a network game with the most players: each god and the neutral player
inline constexpr size_t k_PlayerNumbers = k_MaxPlayers + 1;

static_assert(k_MaxPlayers >= k_LandSlots, "A network game has at least the player slots of a land");
static_assert(k_LandGods + 1 == k_LandSlots, "The neutral player is the last player slot of a land");
static_assert(k_PlayerNumbers <= 0xFFu, "A player number fits in the type of the player names");

/// The player of a god slot of a network game. The slots count the gods in the order of the game room, from 0. Each
/// slot before the neutral player has the god of the same number. Each later slot has the number after it.
[[nodiscard]] constexpr std::optional<PlayerNames> PlayerOfSlot(size_t slot) noexcept
{
	if (slot >= k_MaxPlayers)
	{
		return std::nullopt;
	}
	return static_cast<PlayerNames>(slot < k_LandGods ? slot : slot + 1);
}

/// The god slot of a player of a network game. The neutral player and a number after the last god have no slot.
[[nodiscard]] constexpr std::optional<size_t> SlotOfPlayer(PlayerNames player) noexcept
{
	const auto number = static_cast<size_t>(player);
	if (player == PlayerNames::NEUTRAL || number >= k_PlayerNumbers)
	{
		return std::nullopt;
	}
	return number < k_LandGods ? number : number - 1;
}

/// True for a player number that a network game with the most players can have
[[nodiscard]] constexpr bool IsPlayerNumber(PlayerNames player) noexcept
{
	return static_cast<size_t>(player) < k_PlayerNumbers;
}

/// True for a player of the slots of a land of the game: a god of the land or the neutral player
[[nodiscard]] constexpr bool IsLandPlayer(PlayerNames player) noexcept
{
	return static_cast<size_t>(player) < k_LandSlots;
}

} // namespace openblack::player_slots
