/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <map>
#include <optional>
#include <span>
#include <string>

#include <SDL_scancode.h>

#include "Common/PlayerProfiles.h"
#include "KeyBindings.h"

/// The rules of the Controls page: the keys the player can bind, the key or mouse input that a press gives, how a new
/// binding takes its key or mouse input from every other action, and how the bindings are kept with the profile
namespace openblack::input::rebinding
{

/// The number that the game gives a key: the keyboard's scan code, with 0x80 added for the extended keys. The profile
/// keeps the keys by these numbers.
[[nodiscard]] std::optional<uint8_t> GameKeyCode(SDL_Scancode key) noexcept;
/// The key of a number that the game gives a key, none for a number that no key has
[[nodiscard]] std::optional<SDL_Scancode> KeyOfGameCode(uint32_t code) noexcept;

/// The name of a key as the computer's system names it for the game: "Ctrl", "Shift", "Space", "F1". The left and the
/// right Ctrl, Shift and Alt keys have the same name.
[[nodiscard]] std::u16string KeyName(SDL_Scancode key);
/// A key with its modifier as the Controls page writes it: "Ctrl + S". Empty for no key.
[[nodiscard]] std::u16string ChordName(const std::optional<KeyChord>& chord);

/// Whether the player can bind a key. The game keeps Escape, Tab, P, F1, F2 and the Enter key of the number pad for
/// itself, and the game ignores a key that it has no number for.
[[nodiscard]] bool IsBindable(SDL_Scancode key) noexcept;

/// The modifier keys held as the player presses a key
struct HeldModifiers
{
	bool shift {false};
	bool ctrl {false};
	bool alt {false};
};

/// The key chord that a key press gives. Only one modifier counts, in this order: Shift, then Ctrl, then Alt. A
/// modifier key that the player presses alone gives itself with no modifier. A Shift key pressed with Ctrl held also
/// gives itself with no modifier, because the first held modifier is Shift.
[[nodiscard]] KeyChord CapturedKey(SDL_Scancode key, HeldModifiers held) noexcept;

/// The mouse input that the buttons held and the wheel give, none when nothing is held or turned. The left button
/// comes first, then the right button, then the middle button, then the wheel. Wheel steps away from the player give
/// the wheel up.
[[nodiscard]] std::optional<MouseInput> CapturedMouse(bool left, bool right, bool middle, int wheelSteps) noexcept;

/// Binds a row to a key chord, or to no key. Each other row with the same key and the same modifier loses its key.
/// The row keeps its mouse input.
void AssignKey(std::span<KeyBinding> bindings, size_t row, std::optional<KeyChord> key);
/// Binds a row to a mouse input. Each row with that mouse input loses it first. The row keeps its key.
void AssignMouse(std::span<KeyBinding> bindings, size_t row, MouseInput mouse);

/// The bindings as the profile keeps them: for each row, its key and modifier by the numbers of the game, and its
/// mouse input
[[nodiscard]] std::map<uint32_t, player_profiles::KeptKey> ToKept(std::span<const KeyBinding> bindings);
/// The bindings from the profile. The game's defaults come first. A row takes the profile's values only when the profile
/// keeps all three of them.
[[nodiscard]] KeyBindingTable FromKept(const std::map<uint32_t, player_profiles::KeptKey>& kept);

} // namespace openblack::input::rebinding
