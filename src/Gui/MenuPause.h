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

/// The pause that the game's menu holds. The menu pauses the game when it opens. When the menu closes, the game runs,
/// also when the game was paused before the menu opened.
class MenuPause
{
public:
	/// Follows the menu for one frame. Returns the pause that the game must get now, or nothing when the pause stays.
	[[nodiscard]] std::optional<bool> Follow(bool menuOpen) noexcept
	{
		if (menuOpen && !_held)
		{
			_held = true;
			return true;
		}
		if (!menuOpen && _held)
		{
			_held = false;
			return false;
		}
		return std::nullopt;
	}

	/// Releases the pause after the menu closes outside a frame, for example when the land changes. Returns the pause
	/// that the game gets, or nothing when the menu held no pause.
	[[nodiscard]] std::optional<bool> Release() noexcept { return Follow(false); }

	/// True while the menu holds the pause
	[[nodiscard]] bool IsHeld() const noexcept { return _held; }

private:
	bool _held {false};
};

} // namespace openblack::gui
