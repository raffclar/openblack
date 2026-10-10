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
/// Kept in a key event's spare field
constexpr uint32_t k_InjectedKey = 0x0B1A0B1Au;
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
		return event.motion.which == k_InjectedMouse;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		return event.button.which == k_InjectedMouse;
	case SDL_MOUSEWHEEL:
		return event.wheel.which == k_InjectedMouse;
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		return event.key.keysym.unused == k_InjectedKey;
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
