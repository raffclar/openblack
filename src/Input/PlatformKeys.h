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

#include <SDL_keycode.h>

#include "Common/Platform.h"

/// The keys that the options screen does not bind, and the Escape key, for each platform. A web browser keeps some key
/// chords for its own commands, such as Ctrl and a digit to change the tab, Ctrl+Tab and F11. The web build uses other
/// chords for these actions. The desktop keys are the same as the game's keys.
namespace openblack::input::platform_keys
{

/// The modifier keys that the player holds with a key
struct Modifiers
{
	bool shift {false};
	bool ctrl {false};
	bool alt {false};

	[[nodiscard]] constexpr bool operator==(const Modifiers&) const = default;
};

/// What a digit key from 1 to 7 does to the camera bookmark of that number
enum class BookmarkKey : uint8_t
{
	None,
	/// Fly the camera to the bookmark
	Jump,
	/// Keep the place of the hand, or the thing under the hand, in the bookmark
	Set,
	/// Clear a bookmark that has a place
	Clear,
	/// Start or stop a follow of the bookmark
	ToggleFollow,
};

/// What a digit key from 1 to 7 does with these modifiers. The speed keys go first (see SpeedKeyFor).
/// - On the desktop: the digit alone jumps, Ctrl sets, Ctrl and Shift clear, and Shift follows.
/// - On the web: the digit alone jumps, Ctrl and Shift set, Ctrl and Alt clear, and Shift follows. The browser uses
///   Ctrl and a digit to change the tab, and some browsers use Alt and a digit too.
[[nodiscard]] constexpr BookmarkKey BookmarkKeyFor(Platform platform, Modifiers held) noexcept
{
	const auto [shift, ctrl, alt] = held;
	if (platform == Platform::Web)
	{
		if (ctrl && shift && !alt)
		{
			return BookmarkKey::Set;
		}
		if (ctrl && alt && !shift)
		{
			return BookmarkKey::Clear;
		}
		if (shift && !ctrl)
		{
			return BookmarkKey::ToggleFollow;
		}
		return !ctrl && !alt ? BookmarkKey::Jump : BookmarkKey::None;
	}
	if (ctrl && shift)
	{
		return BookmarkKey::Clear;
	}
	if (ctrl && !alt)
	{
		return BookmarkKey::Set;
	}
	if (shift)
	{
		return BookmarkKey::ToggleFollow;
	}
	return !ctrl && !alt ? BookmarkKey::Jump : BookmarkKey::None;
}

/// A change of the speed of the game
enum class SpeedKey : uint8_t
{
	None,
	/// Half the speed, to half of the normal speed at the least
	Slower,
	/// Two times the speed, to two times the normal speed at the most
	Faster,
};

/// The change of the speed that a key with these modifiers makes. Only Alt is held, as Shift or Ctrl goes before it.
/// - On the desktop: Alt and 1 make the game slower, and Alt and 2 make it faster.
/// - On the web: Alt and the minus key make the game slower, and Alt and the equals key make it faster. Some browsers
///   use Alt and a digit to change the tab.
[[nodiscard]] constexpr SpeedKey SpeedKeyFor(Platform platform, SDL_Keycode key, Modifiers held) noexcept
{
	if (!held.alt || held.shift || held.ctrl)
	{
		return SpeedKey::None;
	}
	const auto slower = platform == Platform::Web ? SDLK_MINUS : SDLK_1;
	const auto faster = platform == Platform::Web ? SDLK_EQUALS : SDLK_2;
	if (key == slower)
	{
		return SpeedKey::Slower;
	}
	return key == faster ? SpeedKey::Faster : SpeedKey::None;
}

/// A move of the camera between the bookmarks and the towns
enum class BookmarkMove : uint8_t
{
	None,
	/// Fly back to the view before the last jump to a bookmark
	Back,
	/// Jump to the last set bookmark
	Previous,
	/// Jump to the next set bookmark
	Next,
	/// Fly to the next town of the player
	NextTown,
};

/// The move that the Tab or Backspace key with these modifiers makes.
/// - On the desktop: Backspace goes back. Shift and Tab go to the previous bookmark, Ctrl and Tab to the next one, and
///   Tab alone to the next town.
/// - On the web: the same, but Ctrl and Backspace go to the next bookmark. The browser uses Ctrl and Tab to change the
///   tab.
[[nodiscard]] constexpr BookmarkMove BookmarkMoveFor(Platform platform, SDL_Keycode key, Modifiers held) noexcept
{
	if (key == SDLK_BACKSPACE)
	{
		return platform == Platform::Web && held.ctrl ? BookmarkMove::Next : BookmarkMove::Back;
	}
	if (key != SDLK_TAB)
	{
		return BookmarkMove::None;
	}
	if (held.shift)
	{
		return BookmarkMove::Previous;
	}
	return held.ctrl ? BookmarkMove::Next : BookmarkMove::NextTown;
}

/// The key that shows or hides the statistics of the renderer: F11 on the desktop, and Insert on the web, because F11
/// is the full screen key of the browser
[[nodiscard]] constexpr SDL_Keycode RendererStatisticsKey(Platform platform) noexcept
{
	return platform == Platform::Web ? SDLK_INSERT : SDLK_F11;
}

/// What an Escape press does
enum class EscapeOutcome : uint8_t
{
	/// The game frees the pointer and does nothing else
	FreePointer,
	/// The game takes the press, as for its menu
	ToGame,
};

/// What an Escape press does. On the desktop the game always takes it. On the web, the first Escape frees the pointer
/// and the second Escape goes to the game. The browser frees the pointer itself on most Escape presses, and the game
/// does not see these presses. When the browser has a keyboard lock in full screen, the game sees the press and frees
/// the pointer itself.
[[nodiscard]] constexpr EscapeOutcome EscapeOutcomeFor(Platform platform, bool pointerHeld) noexcept
{
	return platform == Platform::Web && pointerHeld ? EscapeOutcome::FreePointer : EscapeOutcome::ToGame;
}

/// The pointer lock of the web page as the game sees it. The page tells when the lock starts and stops. The game can
/// ask the page to free the pointer, and from then it does not hold the pointer, before the page tells the stop.
class PointerHold
{
public:
	/// The page told that the pointer lock started or stopped
	constexpr void SetLocked(bool locked) noexcept
	{
		if (locked != _locked)
		{
			_freeAsked = false;
		}
		_locked = locked;
	}
	/// The game asked the page to free the pointer
	constexpr void AskFree() noexcept { _freeAsked = _locked; }
	/// True while the game holds the pointer
	[[nodiscard]] constexpr bool Held() const noexcept { return _locked && !_freeAsked; }

private:
	bool _locked {false};
	bool _freeAsked {false};
};

} // namespace openblack::input::platform_keys
