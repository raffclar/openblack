/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InjectedInput.h"

#include <cstdint>

#include <SDL_events.h>

namespace
{
/// The device the game's own mouse events come from: no mouse SDL knows has this number, and it isn't the touch mouse
constexpr uint32_t k_InjectedMouse = 0x0B1A0B1Au;
/// The device the game's own mouse events come from when it takes their state as they are pumped, as from a real mouse
constexpr uint32_t k_PumpedMouse = 0x0B1A0B1Bu;
/// Kept in a key event's spare field
constexpr uint32_t k_InjectedKey = 0x0B1A0B1Au;

bool IsInjectedMouse(uint32_t which)
{
	return which == k_InjectedMouse || which == k_PumpedMouse;
}
} // namespace

void openblack::input::MarkInjected(SDL_Event& event)
{
	switch (event.type)
	{
	case SDL_MOUSEMOTION:
		event.motion.which = k_InjectedMouse;
		break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		event.button.which = k_InjectedMouse;
		break;
	case SDL_MOUSEWHEEL:
		event.wheel.which = k_InjectedMouse;
		break;
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		event.key.keysym.unused = k_InjectedKey;
		break;
	default:
		break;
	}
}

bool openblack::input::IsInjected(const SDL_Event& event)
{
	switch (event.type)
	{
	case SDL_MOUSEMOTION:
		return IsInjectedMouse(event.motion.which);
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		return IsInjectedMouse(event.button.which);
	case SDL_MOUSEWHEEL:
		return IsInjectedMouse(event.wheel.which);
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		return event.key.keysym.unused == k_InjectedKey;
	default:
		return false;
	}
}

void openblack::input::MarkPumped(SDL_Event& event)
{
	switch (event.type)
	{
	case SDL_MOUSEMOTION:
		event.motion.which = k_PumpedMouse;
		break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		event.button.which = k_PumpedMouse;
		break;
	default:
		MarkInjected(event);
		break;
	}
}

bool openblack::input::IsPumped(const SDL_Event& event)
{
	switch (event.type)
	{
	case SDL_MOUSEMOTION:
		return event.motion.which == k_PumpedMouse;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		return event.button.which == k_PumpedMouse;
	default:
		return false;
	}
}

bool openblack::input::IsPlayerDeviceEvent(const SDL_Event& event)
{
	switch (event.type)
	{
	case SDL_MOUSEMOTION:
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEWHEEL:
	case SDL_KEYDOWN:
	case SDL_KEYUP:
	case SDL_TEXTINPUT:
	case SDL_TEXTEDITING:
	case SDL_FINGERDOWN:
	case SDL_FINGERUP:
	case SDL_FINGERMOTION:
	case SDL_MULTIGESTURE:
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
	case SDL_CONTROLLERAXISMOTION:
	case SDL_JOYAXISMOTION:
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		return true;
	default:
		return false;
	}
}
