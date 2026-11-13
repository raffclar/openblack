/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// When the front end's pointer shows over the game. The pointer shows while a box of the front end is the box that
/// takes the mouse. Then the player's hand is away: the game draws the pointer in place of the hand, also in the temple.
namespace openblack::gui::front_end_pointer
{

/// The boxes of the front end as they are this frame
struct Boxes
{
	/// A box takes the mouse for all the time that it shows: the menu, the start-of-game question, the tattoo editor,
	/// the new player's box, the skirmish box, the questions and the save name box, the staff question, the boxes of the
	/// online front end, the end box or the land information box
	bool boxUp {false};
	/// Controls with no box behind them take the mouse only while the pointer is over one of them, or while a press that
	/// started on one of them is held. These are the chat, the custom music controls, the incoming message and the
	/// buttons of the story history.
	bool controlsTakeMouse {false};
};

/// Whether the front end's pointer shows, and the hand is away
[[nodiscard]] constexpr bool IsOn(const Boxes& boxes)
{
	return boxes.boxUp || boxes.controlsTakeMouse;
}

} // namespace openblack::gui::front_end_pointer
