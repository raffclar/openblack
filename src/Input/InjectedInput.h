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

#include <bitset>

#include <SDL_keycode.h>
#include <SDL_scancode.h>

union SDL_Event;

/// Telling the input the game makes for itself (the testbed's scenarios, the debug inspector driving it for an agent)
/// from the player's mouse and keyboard, so that the player's can be kept out while an agent drives the game
namespace openblack::input
{

/// Marks a mouse or keyboard event made by the game itself rather than by the player's devices
void MarkInjected(SDL_Event& event);
/// Whether a mouse or keyboard event was made by the game itself
[[nodiscard]] bool IsInjected(const SDL_Event& event);
/// Marks a mouse move or button made by the game itself whose state (where the pointer is, the buttons held) the game
/// takes only as the event is pumped from the window's queue, as SDL takes a real mouse's. Such an event is injected too.
/// Other events are marked injected only.
void MarkPumped(SDL_Event& event);
/// Whether a mouse event carries the pointer's state to take as it is pumped
[[nodiscard]] bool IsPumped(const SDL_Event& event);
/// Whether an event is the player's mouse or keyboard at work: moving, buttons, the wheel, keys and typing
[[nodiscard]] bool IsPlayerDeviceEvent(const SDL_Event& event);

/// The modifiers that the held keys make, as SDL's modifier bits: each Shift, Ctrl, Alt and system key held sets its bit
[[nodiscard]] uint16_t ModifiersOfHeldKeys(const std::bitset<SDL_NUM_SCANCODES>& held);

/// A key that the game itself presses or lets go of
struct InjectedKey
{
	SDL_Scancode scancode {SDL_SCANCODE_UNKNOWN};
	/// The key that the keyboard's layout puts on the scancode
	SDL_Keycode sym {SDLK_UNKNOWN};
	bool down {true};
	bool repeat {false};
	/// The modifiers held after the key went down or up, as a real key's event has them
	uint16_t mod {0};
	uint32_t windowId {0};
};

/// The window's event for a key that the game itself presses or lets go of, marked injected
[[nodiscard]] SDL_Event MakeInjectedKey(const InjectedKey& key);

} // namespace openblack::input
