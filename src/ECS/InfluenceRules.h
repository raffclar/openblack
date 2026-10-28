/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// Rules of the influence of the players that keep no state
namespace openblack::influence_rules
{

/// Whether a player can have influence at all: in a network game, only a player with a temple. In a game alone, every
/// player can, through their towns too.
[[nodiscard]] constexpr bool CanHaveInfluence(bool networkGame, bool hasTemple)
{
	return !networkGame || hasTemple;
}

} // namespace openblack::influence_rules
