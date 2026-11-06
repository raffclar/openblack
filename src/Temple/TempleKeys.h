/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// When the keys of the temple's rooms and the Escape key act
namespace openblack::temple::keys
{

/// What the keys of the temple's rooms depend on
struct RoomKeyFacts
{
	/// The player is in the temple
	bool insideTemple {false};
	/// A script has the cinema bars in
	bool scriptBars {false};
	/// The game is over: the temple of the player is in its destruction
	bool gameOver {false};
	/// The player has a temple that stands wholly built
	bool templeBuilt {false};
};

/// Whether a key of a room takes the player to that room. While a script has the cinema bars in, the keys do nothing.
/// In the temple, the keys do nothing once the game is over. Outside, a key takes the player in only when their temple
/// stands wholly built, or when the game is over.
[[nodiscard]] constexpr bool RoomKeyActs(const RoomKeyFacts& facts) noexcept
{
	if (facts.scriptBars)
	{
		return false;
	}
	if (facts.insideTemple)
	{
		return !facts.gameOver;
	}
	return facts.gameOver || facts.templeBuilt;
}

/// What the Escape key in the temple depends on
struct EscapeFacts
{
	/// The player holds Shift, Ctrl or Alt
	bool modifierHeld {false};
	/// A script has the cinema bars in
	bool scriptBars {false};
	/// The game is over: the temple of the player is in its destruction
	bool gameOver {false};
};

/// Whether Escape in the temple goes back to the main room, or out from the main room. Escape with Shift, Ctrl or Alt
/// does not do this. While a script has the cinema bars in, the temple gets no keys. Once the game is over, Escape goes
/// to the question about a new game, not to the rooms.
[[nodiscard]] constexpr bool EscapeLeavesRoom(const EscapeFacts& facts) noexcept
{
	return !facts.modifierHeld && !facts.scriptBars && !facts.gameOver;
}

} // namespace openblack::temple::keys
