/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <chrono>

#include "InputDeviceInterface.h"

namespace openblack::input
{

/// The input device of a game that has no window system: the dedicated server and the tests. It gives what the window
/// system gives before it starts: no key is down, no window has the keys or the mouse, and the queue takes no events.
/// The names of the keys are the names that the window system has before its keyboard starts.
class NoInputDevice final: public InputDeviceInterface
{
public:
	[[nodiscard]] SDL_Window* KeyboardFocus() const override { return nullptr; }
	[[nodiscard]] SDL_Window* MouseFocus() const override { return nullptr; }
	[[nodiscard]] std::span<const uint8_t> KeyboardState() const override { return _keys; }
	[[nodiscard]] SDL_Keymod ModState() const override { return _mod; }
	void SetModState(SDL_Keymod mod) override { _mod = mod; }
	[[nodiscard]] MouseState Mouse() const override { return {}; }
	[[nodiscard]] uint32_t Ticks() const override;

	void PushEvent([[maybe_unused]] SDL_Event& event) override {}
	int PeepEvents([[maybe_unused]] std::span<SDL_Event> events, [[maybe_unused]] SDL_eventaction action) override
	{
		return -1;
	}

	[[nodiscard]] SDL_Keycode KeyFromScancode([[maybe_unused]] SDL_Scancode scancode) const override { return SDLK_UNKNOWN; }
	[[nodiscard]] std::string_view ScancodeName(SDL_Scancode scancode) const override;
	[[nodiscard]] SDL_Scancode ScancodeFromName(std::string_view name) const override;

	void UseRawRelativeMotion() override {}
	void StartTextInput() override {}
	void WarpPointer([[maybe_unused]] SDL_Window* window, [[maybe_unused]] glm::ivec2 position,
	                 [[maybe_unused]] bool background) override
	{
	}
	/// The window system with no window has no relative mode: it refuses to start it, and it stops it with no fault
	bool SetRelativePointer(bool relative, [[maybe_unused]] bool background) override { return !relative; }

private:
	std::array<uint8_t, SDL_NUM_SCANCODES> _keys {};
	SDL_Keymod _mod = KMOD_NONE;
	std::chrono::steady_clock::time_point _start = std::chrono::steady_clock::now();
};

/// The name of a key before the keyboard of the window system starts, or an empty name
[[nodiscard]] std::string_view DefaultScancodeName(SDL_Scancode scancode);

/// The scancode of a key by its name before the keyboard of the window system starts, in any case of the letters
[[nodiscard]] SDL_Scancode DefaultScancodeFromName(std::string_view name);

} // namespace openblack::input
