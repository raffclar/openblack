/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::input::box_keyboard
{

/// The boxes on the screen that can take the keyboard
struct Boxes
{
	/// The game's menu, with its pages and its questions
	bool menu {false};
	/// The start-of-game question
	bool skipBox {false};
	/// The box that makes a new player
	bool newPlayerBox {false};
	bool skirmishBox {false};
	bool landInfoBox {false};
	/// The boxes that save and load a game, and the question of a network game that is out of step
	bool saveGameBoxes {false};
	bool endGameBox {false};
	/// The boxes of the multiplayer front end
	bool multiplayerBoxes {false};
	bool tattooEditor {false};
	/// The chat of a network game is open and its line has the focus
	bool chatTyping {false};
	/// The buttons of the story history in the temple's library
	bool storyHistory {false};
	/// The controls of the custom music player
	bool musicControls {false};
};

/// True when the box on the screen takes the keyboard. The story history and the music player's controls let the keys
/// go to the game. The chat takes the keyboard only while the player types in its line.
[[nodiscard]] constexpr bool WantsKeys(const Boxes& boxes) noexcept
{
	return boxes.menu || boxes.skipBox || boxes.newPlayerBox || boxes.skirmishBox || boxes.landInfoBox || boxes.saveGameBoxes ||
	       boxes.endGameBox || boxes.multiplayerBoxes || boxes.tattooEditor || boxes.chatTyping;
}

/// Whether a box has the keyboard. The game finds this again only when a key goes down, a repeat of a held key too. So a
/// box that opens while no key goes down does not stop the actions that are held. A box that closes keeps the
/// keyboard until the next key goes down, unless the box is a message box of the game that the player answers.
class Latch
{
public:
	/// A key goes down: the box on the screen now decides
	constexpr void KeyDown(bool boxWantsKeys) noexcept { _taken = boxWantsKeys; }
	/// The player answers a message box of the game, which gives the keyboard back
	constexpr void Answered() noexcept { _taken = false; }
	/// While a box has the keyboard, every action reads as not held, and the camera does not read its keys
	[[nodiscard]] constexpr bool Taken() const noexcept { return _taken; }

private:
	bool _taken {false};
};

} // namespace openblack::input::box_keyboard
