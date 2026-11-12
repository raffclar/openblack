/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

/// The flag of the interface for a press of the select button (the left button) that holds the land.
///
/// The interface keeps the flag only while an action of the hand holds the interface lock. A press of the select button
/// in the normal state of the hand grips the land, and the grip holds the lock. Thus the flag stays from the press until
/// one of these:
/// - the player lets go of the select button;
/// - the player pushes the other button, because both buttons end the action of the hand;
/// - the action of the hand ends for another cause (see hand_send_wait::LockEnd).
///
/// The second press of a double click is a double click message, which does not change the flag. While the interface is
/// off, the action of the hand ends at each step, so a press does not keep the flag.
namespace openblack::input::select_press
{

/// What a button of the mouse did
enum class Event : uint8_t
{
	/// A press of the select button, not the second press of a double click
	SelectPress,
	/// The second press of a double click of the select button
	SelectDoublePress,
	/// The select button is let go
	SelectLetGo,
	/// A press of the other button (the right button), also as a double click
	OtherPress,
	/// Anything else, such as a motion of the mouse or a let go of the other button
	Other,
};

struct Facts
{
	Event event {Event::Other};
	/// The hand takes the presses: the interface is on
	bool interfaceOn {true};
	/// After the event, the select button grips the land and the other button is not down
	bool gripsLandAlone {false};
	/// After the event, the select button is down
	bool selectDown {false};
};

/// The flag after the event, from the flag before it
[[nodiscard]] constexpr bool Next(bool holds, const Facts& facts) noexcept
{
	switch (facts.event)
	{
	case Event::SelectPress:
		return facts.interfaceOn && facts.gripsLandAlone;
	case Event::SelectLetGo:
	case Event::OtherPress:
		return false;
	case Event::SelectDoublePress:
	case Event::Other:
		// A let go that the menu or the debug windows took also lets go of the select button
		return holds && facts.selectDown;
	}
	return false;
}

} // namespace openblack::input::select_press
