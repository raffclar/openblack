/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::hand_visibility
{

/// What decides whether the player's hand is out
struct ShowFacts
{
	/// A dialog shows its own pointer
	bool dialogOpen {false};
	/// The interface is active: no script's cinema bars have taken it
	bool interfaceActive {true};
	/// The player is inside the temple
	bool insideTemple {false};
	/// The boxes of the online front end are up over the temple's future room
	bool onlineFrontEnd {false};
};

/// Whether the player's hand is out. It is put away while a dialog shows its own pointer, and while a script's cinema
/// bars have taken the interface, except inside the temple, whose rooms draw the hand under the cursor whatever the
/// scripts do. The temple does not draw the hand while the online front end is up: its boxes show the front end's pointer
/// until the game starts. Put away, it is drawn nowhere, casts no shadow, stays where it was without following the cursor,
/// and bends no trees; it comes back from where it was.
[[nodiscard]] constexpr bool IsShown(const ShowFacts& facts)
{
	return !facts.dialogOpen && !facts.onlineFrontEnd && (facts.interfaceActive || facts.insideTemple);
}

/// Whether the player's hand shows the effects of the miracle that it holds: the glow, the bands and the bracelets. Only a
/// hand that is out shows them, and never inside the temple. The clocks of these effects move only while the hand shows
/// them. Thus a band in flight stops while the hand is away, and it continues when the hand comes back.
[[nodiscard]] constexpr bool ShowsMiracleEffects(bool shown, bool insideTemple)
{
	return shown && !insideTemple;
}

/// Whether a press of a mouse button reaches the player's hand. While a script's cinema bars have taken the interface the
/// hand does nothing: it grips no land (no dust, no grab sound, no splash), takes and casts nothing, and clicks nothing for
/// the scripts. Letting go of a button still lets go of whatever the hand held.
[[nodiscard]] constexpr bool TakesPresses(bool interfaceActive)
{
	return interfaceActive;
}

} // namespace openblack::hand_visibility
