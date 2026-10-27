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

/// Whether a person or the computer plays a player. Every computer of a game finds the same answer at the same turn.
namespace openblack::player_kind
{

/// What decides whether the computer plays a player
struct Facts
{
	PlayerNames player {PlayerNames::NEUTRAL};
	bool networkGame {false};
	/// A computer mind plays the player, or played it before
	bool computerMind {false};
};

/// Whether the computer plays a player. Nobody plays the neutral player. In a game alone, the person plays the first
/// player and the computer plays the others. In a network game, the computer plays only the players of its minds, and
/// the people at the computers play the other players.
[[nodiscard]] constexpr bool IsComputer(const Facts& facts)
{
	if (facts.player == PlayerNames::NEUTRAL)
	{
		return false;
	}
	return facts.networkGame ? facts.computerMind : facts.player != PlayerNames::PLAYER_ONE;
}

} // namespace openblack::player_kind
