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

#include <array>
#include <optional>
#include <ranges>

/// The order of the turn of the players. Each player does its whole turn before the next player starts: its temple, its
/// towns, its stones, its alignment, its computer mind, the power of its tribes and its influence power. Thus a computer
/// mind sees the players before it as they are in this turn, and the players after it as they were in the last turn. The
/// random numbers come in the same order as the game draws them. The order keeps no game state, so that the game and the
/// tests use it both.
namespace openblack::player_turn
{

/// A part of the turn of one player
enum class Part : uint8_t
{
	/// In a network game, the winning conditions take their values
	Conditions,
	/// The temple of the player, with its destruction and the beam of its heart
	Temple,
	/// Each town of the player, in the order that the player got them
	Towns,
	/// The stones of the player drop the travellers that changed to a different thing
	Stones,
	/// The waiting change of the player moves its alignment
	Alignment,
	/// The computer mind of the player
	Mind,
	/// The power of each tribe for the player becomes the most power that the tribe gave it
	Powers,
	/// The temples of the player take their size target, and the player notes its influence power
	Influence,
};

/// The parts of the turn of one player, in their order
inline constexpr std::array k_Parts {Part::Conditions, Part::Temple, Part::Towns,  Part::Stones,
                                     Part::Alignment,  Part::Mind,   Part::Powers, Part::Influence};

/// Each player in the order of the players does each part of its turn (play) before the next player starts
template <typename Players, typename PartTurn>
void Play(const Players& players, PartTurn&& play)
{
	for (const auto player : players)
	{
		for (const auto part : k_Parts)
		{
			play(player, part);
		}
	}
}

/// Each player from the player at the index first does each part of its turn (play), as Play does. After the whole turn
/// of each player but the last, the order asks cut. When cut is true, the order stops and gives the index of the next
/// player. The order continues from that index later. When all the players did their turns, the order gives no index.
template <typename Players, typename PartTurn, typename Cut>
[[nodiscard]] std::optional<size_t> PlayFrom(const Players& players, size_t first, PartTurn&& play, Cut&& cut)
{
	const auto count = static_cast<size_t>(std::ranges::distance(players));
	size_t index = 0;
	for (const auto player : players)
	{
		if (index < first)
		{
			++index;
			continue;
		}
		for (const auto part : k_Parts)
		{
			play(player, part);
		}
		++index;
		if (index < count && cut())
		{
			return index;
		}
	}
	return std::nullopt;
}

} // namespace openblack::player_turn
