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

#include <algorithm>
#include <array>
#include <utility>

#include <SDL_events.h>
#include <SDL_gamecontroller.h>

namespace
{
/// The device the game's own mouse events come from: no mouse SDL knows has this number, and it isn't the touch mouse
constexpr uint32_t k_InjectedMouse = 0x0B1A0B1Au;
/// The device the game's own mouse events come from when it takes their state as they are pumped, as from a real mouse
constexpr uint32_t k_PumpedMouse = 0x0B1A0B1Bu;
/// The controller that the game's own controller events come from: no controller SDL knows has this number
constexpr int32_t k_InjectedController = 0x0B1A0B1A;
/// Kept in a key event's spare field
constexpr uint32_t k_InjectedKey = 0x0B1A0B1Au;
/// The last byte of the letters of a text event that the game made. The game's text events hold at most 30 bytes and
/// their end, so this byte is free.
constexpr char k_InjectedText = '';

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
	case SDL_TEXTINPUT:
		event.text.text[sizeof(event.text.text) - 1] = k_InjectedText;
		break;
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
		event.cbutton.which = k_InjectedController;
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
	case SDL_TEXTINPUT:
		return event.text.text[sizeof(event.text.text) - 1] == k_InjectedText;
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
		return event.cbutton.which == k_InjectedController;
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

uint16_t openblack::input::ModifiersOfHeldKeys(const std::bitset<SDL_NUM_SCANCODES>& held)
{
	constexpr std::array<std::pair<SDL_Scancode, uint16_t>, 8> k_ModifierKeys {{
	    {SDL_SCANCODE_LSHIFT, KMOD_LSHIFT},
	    {SDL_SCANCODE_RSHIFT, KMOD_RSHIFT},
	    {SDL_SCANCODE_LCTRL, KMOD_LCTRL},
	    {SDL_SCANCODE_RCTRL, KMOD_RCTRL},
	    {SDL_SCANCODE_LALT, KMOD_LALT},
	    {SDL_SCANCODE_RALT, KMOD_RALT},
	    {SDL_SCANCODE_LGUI, KMOD_LGUI},
	    {SDL_SCANCODE_RGUI, KMOD_RGUI},
	}};
	uint16_t mod = KMOD_NONE;
	for (const auto& [key, bit] : k_ModifierKeys)
	{
		if (held.test(static_cast<size_t>(key)))
		{
			mod |= bit;
		}
	}
	return mod;
}

std::optional<uint8_t> openblack::input::ControllerButtonNamed(std::string_view name)
{
	constexpr std::array<std::pair<std::string_view, uint8_t>, 15> k_Buttons {{
	    {"a", SDL_CONTROLLER_BUTTON_A},
	    {"b", SDL_CONTROLLER_BUTTON_B},
	    {"x", SDL_CONTROLLER_BUTTON_X},
	    {"y", SDL_CONTROLLER_BUTTON_Y},
	    {"back", SDL_CONTROLLER_BUTTON_BACK},
	    {"guide", SDL_CONTROLLER_BUTTON_GUIDE},
	    {"start", SDL_CONTROLLER_BUTTON_START},
	    {"leftstick", SDL_CONTROLLER_BUTTON_LEFTSTICK},
	    {"rightstick", SDL_CONTROLLER_BUTTON_RIGHTSTICK},
	    {"leftshoulder", SDL_CONTROLLER_BUTTON_LEFTSHOULDER},
	    {"rightshoulder", SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},
	    {"dpup", SDL_CONTROLLER_BUTTON_DPAD_UP},
	    {"dpdown", SDL_CONTROLLER_BUTTON_DPAD_DOWN},
	    {"dpleft", SDL_CONTROLLER_BUTTON_DPAD_LEFT},
	    {"dpright", SDL_CONTROLLER_BUTTON_DPAD_RIGHT},
	}};
	const auto found = std::ranges::find(k_Buttons, name, &std::pair<std::string_view, uint8_t>::first);
	return found != k_Buttons.end() ? std::optional(found->second) : std::nullopt;
}

SDL_Event openblack::input::MakeInjectedKey(const InjectedKey& key)
{
	SDL_Event event {};
	event.type = key.down ? SDL_KEYDOWN : SDL_KEYUP;
	event.key.windowID = key.windowId;
	event.key.state = key.down ? SDL_PRESSED : SDL_RELEASED;
	event.key.repeat = key.repeat ? 1 : 0;
	event.key.keysym.scancode = key.scancode;
	event.key.keysym.sym = key.sym;
	event.key.keysym.mod = key.mod;
	MarkInjected(event);
	return event;
}
