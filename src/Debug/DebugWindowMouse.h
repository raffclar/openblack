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

#include <SDL_events.h>

#include "Input/InjectedInput.h"

namespace openblack::debug::gui
{

/// The mouse events that the debug windows take
enum class WindowMouse : uint8_t
{
	/// The windows take the mouse of the player
	Player,
	/// The windows take only the mouse events that the game or a tool sends. The inspector drives the pointer, or the
	/// input of the player is locked.
	DrivenOnly,
	/// The windows take no press and no turn of the wheel, because a scenario or a hand demonstration drives the hand
	KeptOff,
};

/// The sources that can drive the mouse of the debug windows
struct WindowMouseDrivers
{
	/// A scenario drives the hand
	bool scenarioDrivesHand;
	/// A hand demonstration plays its recorded mouse
	bool handDemoPlays;
	/// The inspector drives the pointer
	bool pointerDriven;
	/// The input of the player is locked
	bool inputLocked;
};

/// The mouse events that the debug windows take. A scenario that drives the hand and a hand demonstration have priority
/// over the inspector and the lock. Then a debug window under the pointer does not take the presses of the hand.
[[nodiscard]] constexpr WindowMouse ChooseWindowMouse(const WindowMouseDrivers& drivers) noexcept
{
	if (drivers.scenarioDrivesHand || drivers.handDemoPlays)
	{
		return WindowMouse::KeptOff;
	}
	return drivers.pointerDriven || drivers.inputLocked ? WindowMouse::DrivenOnly : WindowMouse::Player;
}

/// Whether the event comes from the mouse, or tells that the pointer of the system went into or out of the window
[[nodiscard]] inline bool IsMouseEvent(const SDL_Event& event) noexcept
{
	switch (event.type)
	{
	case SDL_MOUSEMOTION:
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEWHEEL:
		return true;
	case SDL_WINDOWEVENT:
		return event.window.event == SDL_WINDOWEVENT_ENTER || event.window.event == SDL_WINDOWEVENT_LEAVE;
	default:
		return false;
	}
}

/// Whether the debug windows take the event. Events that do not come from the mouse always go to the windows.
/// - With the mouse of the player, the windows take all events.
/// - With the driven pointer only, the windows take the mouse events that the game or a tool sends. The real pointer
///   does not move the pointer of the windows, also when the window of the game has the focus.
/// - Kept off the mouse, the windows take no press and no turn of the wheel. A release still goes to the windows, so
///   that no button of the windows stays down.
[[nodiscard]] inline bool WindowsTakeEvent(const SDL_Event& event, WindowMouse mouse) noexcept
{
	if (!IsMouseEvent(event))
	{
		return true;
	}
	switch (mouse)
	{
	case WindowMouse::Player:
		return true;
	case WindowMouse::DrivenOnly:
		return event.type != SDL_WINDOWEVENT && input::IsInjected(event);
	case WindowMouse::KeptOff:
		return event.type != SDL_MOUSEBUTTONDOWN && event.type != SDL_MOUSEWHEEL;
	}
	return true;
}

} // namespace openblack::debug::gui
