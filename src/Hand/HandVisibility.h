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

/// Whether the player's hand is out. It is put away while a dialog shows its own pointer, and while a script's cinema
/// bars have taken the interface. Put away, it is drawn nowhere, casts no shadow, stays where it was without following
/// the cursor, and bends no trees; it comes back from where it was.
[[nodiscard]] constexpr bool IsShown(bool dialogOpen, bool interfaceActive)
{
	return !dialogOpen && interfaceActive;
}

} // namespace openblack::hand_visibility
