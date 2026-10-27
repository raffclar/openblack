/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Enums.h"

/// Whether a person, the computer or nobody plays a player. Every computer of a game finds the same answer at the same
/// turn.
namespace openblack::player_kind
{

/// What decides who plays a player
struct Facts
{
	PlayerNames player {PlayerNames::NEUTRAL};
	bool networkGame {false};
	/// A computer mind plays the player, or played it before
	bool computerMind {false};
	/// The player has hands of its own. In a network game, a person who joins the game gets the hands of a player.
	bool hands {false};
};

/// Who plays a player
enum class Kind
{
	/// Nobody plays the player
	None,
	/// A person plays the player
	Human,
	/// A computer mind plays the player
	Computer,
};

/// Who plays a player:
/// - Nobody plays the neutral player.
/// - A computer mind that a land script or the start of the game switched on makes its player a computer player. This
///   is true in every game, also for the first player in a game alone. The player stays a computer player.
/// - In a game alone, the person plays the first player, and nobody plays the other players.
/// - In a network game, the people play the players that have hands of their own.
[[nodiscard]] constexpr Kind KindOf(const Facts& facts)
{
	if (facts.player == PlayerNames::NEUTRAL)
	{
		return Kind::None;
	}
	if (facts.computerMind)
	{
		return Kind::Computer;
	}
	const bool person = facts.networkGame ? facts.hands : facts.player == PlayerNames::PLAYER_ONE;
	return person ? Kind::Human : Kind::None;
}

/// Whether the computer plays a player. In a game alone, the computer plays each player that the person does not play.
/// In a network game, the computer plays only the players of its minds.
[[nodiscard]] constexpr bool IsComputer(const Facts& facts)
{
	const auto kind = KindOf(facts);
	return kind == Kind::Computer || (kind == Kind::None && !facts.networkGame && facts.player != PlayerNames::NEUTRAL);
}

/// Whether a person plays a player
[[nodiscard]] constexpr bool IsHuman(const Facts& facts)
{
	return KindOf(facts) == Kind::Human;
}

} // namespace openblack::player_kind
