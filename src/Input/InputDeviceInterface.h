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

#include <span>
#include <string_view>

#include <SDL_events.h>
#include <SDL_keyboard.h>
#include <SDL_keycode.h>
#include <SDL_scancode.h>
#include <glm/vec2.hpp>

struct SDL_Window;

namespace openblack::input
{

/// The keyboard, the mouse, the pointer and the event queue of the system. The world reads and writes them only
/// through this interface. The game has the device of its window system. The dedicated server has no device: there the
/// world gets a device that has no input (NoInputDevice), so that the server does not link the window system.
class InputDeviceInterface
{
public:
	/// The buttons that the mouse holds, as a mask of SDL_BUTTON values, and the place of the pointer in the window
	struct MouseState
	{
		uint32_t buttons = 0;
		glm::ivec2 position {0, 0};
	};

	InputDeviceInterface() = default;
	InputDeviceInterface(const InputDeviceInterface&) = delete;
	InputDeviceInterface& operator=(const InputDeviceInterface&) = delete;
	InputDeviceInterface(InputDeviceInterface&&) = delete;
	InputDeviceInterface& operator=(InputDeviceInterface&&) = delete;
	virtual ~InputDeviceInterface() = default;

	/// The window that has the keys, or null
	[[nodiscard]] virtual SDL_Window* KeyboardFocus() const = 0;
	/// The window that has the mouse, or null
	[[nodiscard]] virtual SDL_Window* MouseFocus() const = 0;
	/// For each key, by its scancode, 1 when the key is down and 0 when it is up
	[[nodiscard]] virtual std::span<const uint8_t> KeyboardState() const = 0;
	/// The modifier keys that are down
	[[nodiscard]] virtual SDL_Keymod ModState() const = 0;
	virtual void SetModState(SDL_Keymod mod) = 0;
	[[nodiscard]] virtual MouseState Mouse() const = 0;
	/// The milliseconds of the machine's clock from the start of the device
	[[nodiscard]] virtual uint32_t Ticks() const = 0;

	/// Adds an event to the end of the queue
	virtual void PushEvent(SDL_Event& event) = 0;
	/// Adds, reads or removes the events of all types, as SDL_PeepEvents does. With an empty span and a read, it gives
	/// the number of events in the queue. A negative number tells that the queue does not work.
	virtual int PeepEvents(std::span<SDL_Event> events, SDL_eventaction action) = 0;

	/// The key that the scancode gives in the layout of the keyboard
	[[nodiscard]] virtual SDL_Keycode KeyFromScancode(SDL_Scancode scancode) const = 0;
	/// The name of the key with the scancode, or an empty name
	[[nodiscard]] virtual std::string_view ScancodeName(SDL_Scancode scancode) const = 0;
	/// The scancode of the key with a name, in upper or lower case, or SDL_SCANCODE_UNKNOWN
	[[nodiscard]] virtual SDL_Scancode ScancodeFromName(std::string_view name) const = 0;

	/// Gives the movements of the mouse in relative mode without the speed and the acceleration of the system's pointer
	virtual void UseRawRelativeMotion() = 0;
	/// Starts the text input of the window, so that the keys also give text events
	virtual void StartTextInput() = 0;
	/// Puts the pointer of the system at a place in the window (see windowing::system_pointer)
	virtual void WarpPointer(SDL_Window* window, glm::ivec2 position, bool background) = 0;
	/// Holds the pointer of the system in the window, or lets it go. Gives true when the mode changed.
	virtual bool SetRelativePointer(bool relative, bool background) = 0;
};

/// The input device of the game, or a device that has no input when the game has no device (the dedicated server, a
/// test that makes none)
[[nodiscard]] InputDeviceInterface& Device();

} // namespace openblack::input
