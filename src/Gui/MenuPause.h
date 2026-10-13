/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

namespace openblack::gui
{

/// The pause that the game's menu holds. The menu pauses the game when it opens. When the menu closes, the game gets
/// the pause that it had before the menu opened.
class MenuPause
{
public:
	/// Follows the menu for one frame. Returns the pause that the game must get now, or nothing when the pause stays.
	[[nodiscard]] std::optional<bool> Follow(bool menuOpen, bool gamePaused) noexcept
	{
		if (menuOpen && !_held)
		{
			_held = true;
			_pausedBefore = gamePaused;
			return true;
		}
		if (!menuOpen && _held)
		{
			_held = false;
			return _pausedBefore;
		}
		return std::nullopt;
	}

	/// Releases the pause after the menu closes outside a frame, for example when the land changes. Returns the pause
	/// that the game had before the menu opened, or nothing when the menu held no pause.
	[[nodiscard]] std::optional<bool> Release() noexcept { return Follow(false, false); }

	/// True while the menu holds the pause
	[[nodiscard]] bool IsHeld() const noexcept { return _held; }

private:
	bool _held {false};
	bool _pausedBefore {false};
};

} // namespace openblack::gui
