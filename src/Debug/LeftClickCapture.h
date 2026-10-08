/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::debug
{

/// Whether a debug window takes a left button press or release away from the game. A press is taken while the window
/// awaits a click and the mouse is not over the GUI; the release of a press it took is taken too, even once the click is
/// no longer awaited, because the game keeps the left button as a toggle and a release on its own would leave it held.
struct LeftClickCapture
{
	/// A press was taken and its release has not come yet
	bool releasePending {false};

	[[nodiscard]] bool Takes(bool press, bool awaitingClick, bool overGui) const
	{
		return press ? awaitingClick && !overGui : releasePending;
	}
	/// After the window has seen a left press (`press`) or release it takes
	void Seen(bool press) { releasePending = press; }
};

} // namespace openblack::debug
