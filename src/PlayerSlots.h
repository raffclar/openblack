/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <charconv>
#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>

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

/// The count of the player numbers of a game: the player slots of a land, or more when a network game has more gods.
/// The count is never more than the player numbers of a network game with the most players.
[[nodiscard]] constexpr size_t PlayerNumbersOf(size_t count) noexcept
{
	return std::clamp(count, k_LandSlots, k_PlayerNumbers);
}

/// Each player of a game with the given count of player numbers, in the order of their numbers. The neutral player
/// comes after the gods of a land and before the later gods.
[[nodiscard]] constexpr auto Players(size_t count)
{
	return std::views::iota(size_t {0}, PlayerNumbersOf(count)) |
	       std::views::transform([](size_t number) { return static_cast<PlayerNames>(number); });
}

/// Each god of a game with the given count of player numbers, in the order of their numbers: each player but the
/// neutral player
[[nodiscard]] constexpr auto Gods(size_t count)
{
	return Players(count) | std::views::filter([](PlayerNames player) { return player != PlayerNames::NEUTRAL; });
}

/// The first part of the name of a god after the seventh in a land's script: PLAYER_ and the player number
inline constexpr std::string_view k_LaterGodNameStart = "PLAYER_";

/// The name of a player in a land's script. The players of a land have the names of the game (PLAYER_ONE to
/// PLAYER_SEVEN and NEUTRAL). A god after the seventh is PLAYER_ and its number: PLAYER_8 to PLAYER_32 (deviation 165).
[[nodiscard]] inline std::string ScriptNameOf(PlayerNames player)
{
	if (IsLandPlayer(player))
	{
		return std::string(k_PlayerNamesStrs.at(static_cast<size_t>(player)));
	}
	return std::string(k_LaterGodNameStart) + std::to_string(static_cast<size_t>(player));
}

/// The player that a name of a land's script names, in upper case as the scripts write it. None for a name that is no
/// player's.
[[nodiscard]] inline std::optional<PlayerNames> PlayerOfScriptName(std::string_view name)
{
	if (const auto land = std::ranges::find(k_PlayerNamesStrs, name); land != k_PlayerNamesStrs.end())
	{
		return static_cast<PlayerNames>(std::distance(k_PlayerNamesStrs.begin(), land));
	}
	if (!name.starts_with(k_LaterGodNameStart))
	{
		return std::nullopt;
	}
	const auto digits = name.substr(k_LaterGodNameStart.size());
	size_t number = 0;
	const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), number);
	if (digits.empty() || error != std::errc {} || end != digits.data() + digits.size() || number < k_LandSlots ||
	    number >= k_PlayerNumbers || digits.front() == '0')
	{
		return std::nullopt;
	}
	return static_cast<PlayerNames>(number);
}

} // namespace openblack::player_slots
