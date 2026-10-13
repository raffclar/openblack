/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// What of the player's interface the game leaves off the screen while the cinema bars are in. The bars come in two
/// ways: a script's cut scene brings them in and holds them, or the game brings them in for itself (nobody holding
/// them). A script's cut scene hides the creature's status panel and the hand's tooltip; the numbers floating up from
/// things are hidden whenever the bars are in, whoever brought them.
namespace openblack::gui::cinema_hides
{

/// The cinema bars as the interface sees them this frame
struct Bars
{
	/// The bars are in, or coming in
	bool in {false};
	/// A script brought them in and holds them
	bool heldByScript {false};
};

/// The creature's status panel shows only outside a script's cut scene
[[nodiscard]] constexpr bool ShowsCreaturePanel(Bars bars)
{
	return !(bars.in && bars.heldByScript);
}

/// The hand's tooltip shows only outside a script's cut scene
[[nodiscard]] constexpr bool ShowsToolTip(Bars bars)
{
	return !(bars.in && bars.heldByScript);
}

/// The numbers floating up from things are drawn only while the bars are out; they go on rising and fading unseen
[[nodiscard]] constexpr bool ShowsFloatingNumbers(Bars bars)
{
	return !bars.in;
}

} // namespace openblack::gui::cinema_hides
