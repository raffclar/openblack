/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "InputDeviceInterface.h"

namespace openblack::input
{

/// The input device of the game's window system, SDL. Each function calls the SDL function of the same name.
class SdlInputDevice final: public InputDeviceInterface
{
public:
	[[nodiscard]] SDL_Window* KeyboardFocus() const override;
	[[nodiscard]] SDL_Window* MouseFocus() const override;
	[[nodiscard]] std::span<const uint8_t> KeyboardState() const override;
	[[nodiscard]] SDL_Keymod ModState() const override;
	void SetModState(SDL_Keymod mod) override;
	[[nodiscard]] MouseState Mouse() const override;
	[[nodiscard]] uint32_t Ticks() const override;

	void PushEvent(SDL_Event& event) override;
	int PeepEvents(std::span<SDL_Event> events, SDL_eventaction action) override;

	[[nodiscard]] SDL_Keycode KeyFromScancode(SDL_Scancode scancode) const override;
	[[nodiscard]] std::string_view ScancodeName(SDL_Scancode scancode) const override;
	[[nodiscard]] SDL_Scancode ScancodeFromName(std::string_view name) const override;

	void UseRawRelativeMotion() override;
	void StartTextInput() override;
	void WarpPointer(SDL_Window* window, glm::ivec2 position, bool background) override;
	bool SetRelativePointer(bool relative, bool background) override;
};

} // namespace openblack::input
