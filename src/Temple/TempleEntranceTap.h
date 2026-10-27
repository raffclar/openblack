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
	/// The game is a network game, where the scripts cannot stop the tap
	bool networkGame;
	/// The entrance is part of the temple of the player that taps it
	bool ownTemple;
	/// The hand that taps is the hand of this computer's player. The game server and an observer have no such hand.
	bool byThisComputer;
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

/// A tap on a temple's entrance. A held leash takes the tap first. In a game on one computer, the scripts must let the
/// player tap the entrance. Only the player's own entrance takes them into the temple, and only on the computer of that
/// player: every computer of a network game plays the tap, but only one computer goes into the temple.
[[nodiscard]] constexpr EntranceTap TapEntrance(const EntranceTapFacts& facts)
{
	const bool tapped = (facts.networkGame || facts.scriptLetsTap) && !facts.leashHeld;
	return {.tapped = tapped, .enters = tapped && facts.ownTemple && facts.byThisComputer};
}

} // namespace openblack::temple
