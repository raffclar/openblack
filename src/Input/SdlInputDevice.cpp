/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SdlInputDevice.h"

#include <string>

#include <SDL_hints.h>
#include <SDL_mouse.h>
#include <SDL_timer.h>

#include "Windowing/SystemPointer.h"

using namespace openblack::input;

SDL_Window* SdlInputDevice::KeyboardFocus() const
{
	return SDL_GetKeyboardFocus();
}

SDL_Window* SdlInputDevice::MouseFocus() const
{
	return SDL_GetMouseFocus();
}

std::span<const uint8_t> SdlInputDevice::KeyboardState() const
{
	int count = 0;
	const auto* state = SDL_GetKeyboardState(&count);
	return {state, static_cast<size_t>(count)};
}

SDL_Keymod SdlInputDevice::ModState() const
{
	return SDL_GetModState();
}

void SdlInputDevice::SetModState(SDL_Keymod mod)
{
	SDL_SetModState(mod);
}

InputDeviceInterface::MouseState SdlInputDevice::Mouse() const
{
	MouseState state;
	state.buttons = SDL_GetMouseState(&state.position.x, &state.position.y);
	return state;
}

uint32_t SdlInputDevice::Ticks() const
{
	return SDL_GetTicks();
}

void SdlInputDevice::PushEvent(SDL_Event& event)
{
	SDL_PushEvent(&event);
}

int SdlInputDevice::PeepEvents(std::span<SDL_Event> events, SDL_eventaction action)
{
	return SDL_PeepEvents(events.data(), static_cast<int>(events.size()), action, SDL_FIRSTEVENT, SDL_LASTEVENT);
}

SDL_Keycode SdlInputDevice::KeyFromScancode(SDL_Scancode scancode) const
{
	return SDL_GetKeyFromScancode(scancode);
}

std::string_view SdlInputDevice::ScancodeName(SDL_Scancode scancode) const
{
	return SDL_GetScancodeName(scancode);
}

SDL_Scancode SdlInputDevice::ScancodeFromName(std::string_view name) const
{
	return SDL_GetScancodeFromName(std::string(name).c_str());
}

void SdlInputDevice::UseRawRelativeMotion()
{
	SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, "0");
}

void SdlInputDevice::StartTextInput()
{
	SDL_StartTextInput();
}

void SdlInputDevice::WarpPointer(SDL_Window* window, glm::ivec2 position, bool background)
{
	windowing::system_pointer::Warp(window, position, background);
}

bool SdlInputDevice::SetRelativePointer(bool relative, bool background)
{
	return windowing::system_pointer::SetRelative(relative, background);
}
