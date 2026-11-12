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

/// Whether Escape in the temple asks "Are you sure you want to start a completely new game?". It asks once the game is
/// over, with Shift, Ctrl or Alt too, but not while a script has the cinema bars in.
[[nodiscard]] constexpr bool EscapeAsksNewGame(const EscapeFacts& facts) noexcept
{
	return facts.gameOver && !facts.scriptBars;
}

/// What a press in a room of pictures depends on
struct PictureRoomPressFacts
{
	/// The game is over: the temple of the player is in its destruction
	bool gameOver {false};
	/// A box, such as the menu or a question, is on the screen
	bool boxUp {false};
};

/// Whether the camera of a room of pictures gets the press of the player. Once the game is over, the room ignores the
/// press while a box is on the screen.
[[nodiscard]] constexpr bool PictureRoomTakesPress(const PictureRoomPressFacts& facts) noexcept
{
	return !(facts.gameOver && facts.boxUp);
}

/// What a click on the door of a room of pictures does
enum class PictureDoorClick
{
	/// The camera walks back through the door to the main room
	WalkBack,
	/// The player gets the question "Are you sure you want to start a completely new game?"
	AskNewGame
};

/// What a click on the door of a room of pictures does. Once the game is over, the click asks about a new game, and the
/// player stays in the room.
[[nodiscard]] constexpr PictureDoorClick PictureDoorClickOutcome(bool gameOver) noexcept
{
	return gameOver ? PictureDoorClick::AskNewGame : PictureDoorClick::WalkBack;
}

} // namespace openblack::temple::keys
