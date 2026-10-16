/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::temple
{

/// The facts about a tap of the hand on a temple's entrance. The hand taps a thing only when it is empty and in the
/// player's influence, so these facts are about the entrance and the leash.
struct EntranceTapFacts
{
	/// The scripts let the player tap the entrance
	bool scriptLetsTap;
	/// The entrance is part of the temple of the player that taps it
	bool ownTemple;
	/// The hand holds the loose leash of its creature, which takes the tap as an order
	bool leashHeld;
};

/// What a tap on a temple's entrance does
struct EntranceTap
{
	/// The tap gets to the entrance, and the help counts it
	bool tapped;
	/// The tap takes the player into their temple
	bool enters;
};

/// A tap on a temple's entrance. A held leash takes the tap first. The scripts must let the player tap the entrance. Only
/// the player's own entrance takes them into the temple.
[[nodiscard]] constexpr EntranceTap TapEntrance(const EntranceTapFacts& facts)
{
	const bool tapped = facts.scriptLetsTap && !facts.leashHeld;
	return {.tapped = tapped, .enters = tapped && facts.ownTemple};
}

} // namespace openblack::temple
