/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "KeyRebinding.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace openblack::input::rebinding
{

namespace
{

/// A key: the number that the game gives it, the key as SDL names it, and the name that the system gives it for the
/// game. The game asks for a name with no difference between the left and the right modifier keys.
struct GameKey
{
	uint8_t code;
	SDL_Scancode key;
	std::u16string_view name;
};

// clang-format off
constexpr std::array k_GameKeys {
	GameKey {0x01, SDL_SCANCODE_ESCAPE, u"Esc"},
	GameKey {0x02, SDL_SCANCODE_1, u"1"},
	GameKey {0x03, SDL_SCANCODE_2, u"2"},
	GameKey {0x04, SDL_SCANCODE_3, u"3"},
	GameKey {0x05, SDL_SCANCODE_4, u"4"},
	GameKey {0x06, SDL_SCANCODE_5, u"5"},
	GameKey {0x07, SDL_SCANCODE_6, u"6"},
	GameKey {0x08, SDL_SCANCODE_7, u"7"},
	GameKey {0x09, SDL_SCANCODE_8, u"8"},
	GameKey {0x0A, SDL_SCANCODE_9, u"9"},
	GameKey {0x0B, SDL_SCANCODE_0, u"0"},
	GameKey {0x0C, SDL_SCANCODE_MINUS, u"-"},
	GameKey {0x0D, SDL_SCANCODE_EQUALS, u"="},
	GameKey {0x0E, SDL_SCANCODE_BACKSPACE, u"Backspace"},
	GameKey {0x0F, SDL_SCANCODE_TAB, u"Tab"},
	GameKey {0x10, SDL_SCANCODE_Q, u"Q"},
	GameKey {0x11, SDL_SCANCODE_W, u"W"},
	GameKey {0x12, SDL_SCANCODE_E, u"E"},
	GameKey {0x13, SDL_SCANCODE_R, u"R"},
	GameKey {0x14, SDL_SCANCODE_T, u"T"},
	GameKey {0x15, SDL_SCANCODE_Y, u"Y"},
	GameKey {0x16, SDL_SCANCODE_U, u"U"},
	GameKey {0x17, SDL_SCANCODE_I, u"I"},
	GameKey {0x18, SDL_SCANCODE_O, u"O"},
	GameKey {0x19, SDL_SCANCODE_P, u"P"},
	GameKey {0x1A, SDL_SCANCODE_LEFTBRACKET, u"["},
	GameKey {0x1B, SDL_SCANCODE_RIGHTBRACKET, u"]"},
	GameKey {0x1C, SDL_SCANCODE_RETURN, u"Enter"},
	GameKey {0x1D, SDL_SCANCODE_LCTRL, u"Ctrl"},
	GameKey {0x1E, SDL_SCANCODE_A, u"A"},
	GameKey {0x1F, SDL_SCANCODE_S, u"S"},
	GameKey {0x20, SDL_SCANCODE_D, u"D"},
	GameKey {0x21, SDL_SCANCODE_F, u"F"},
	GameKey {0x22, SDL_SCANCODE_G, u"G"},
	GameKey {0x23, SDL_SCANCODE_H, u"H"},
	GameKey {0x24, SDL_SCANCODE_J, u"J"},
	GameKey {0x25, SDL_SCANCODE_K, u"K"},
	GameKey {0x26, SDL_SCANCODE_L, u"L"},
	GameKey {0x27, SDL_SCANCODE_SEMICOLON, u";"},
	GameKey {0x28, SDL_SCANCODE_APOSTROPHE, u"'"},
	GameKey {0x29, SDL_SCANCODE_GRAVE, u"`"},
	GameKey {0x2A, SDL_SCANCODE_LSHIFT, u"Shift"},
	GameKey {0x2B, SDL_SCANCODE_BACKSLASH, u"\\"},
	GameKey {0x2C, SDL_SCANCODE_Z, u"Z"},
	GameKey {0x2D, SDL_SCANCODE_X, u"X"},
	GameKey {0x2E, SDL_SCANCODE_C, u"C"},
	GameKey {0x2F, SDL_SCANCODE_V, u"V"},
	GameKey {0x30, SDL_SCANCODE_B, u"B"},
	GameKey {0x31, SDL_SCANCODE_N, u"N"},
	GameKey {0x32, SDL_SCANCODE_M, u"M"},
	GameKey {0x33, SDL_SCANCODE_COMMA, u","},
	GameKey {0x34, SDL_SCANCODE_PERIOD, u"."},
	GameKey {0x35, SDL_SCANCODE_SLASH, u"/"},
	GameKey {0x36, SDL_SCANCODE_RSHIFT, u"Shift"},
	GameKey {0x37, SDL_SCANCODE_KP_MULTIPLY, u"Num *"},
	GameKey {0x38, SDL_SCANCODE_LALT, u"Alt"},
	GameKey {0x39, SDL_SCANCODE_SPACE, u"Space"},
	GameKey {0x3A, SDL_SCANCODE_CAPSLOCK, u"Caps Lock"},
	GameKey {0x3B, SDL_SCANCODE_F1, u"F1"},
	GameKey {0x3C, SDL_SCANCODE_F2, u"F2"},
	GameKey {0x3D, SDL_SCANCODE_F3, u"F3"},
	GameKey {0x3E, SDL_SCANCODE_F4, u"F4"},
	GameKey {0x3F, SDL_SCANCODE_F5, u"F5"},
	GameKey {0x40, SDL_SCANCODE_F6, u"F6"},
	GameKey {0x41, SDL_SCANCODE_F7, u"F7"},
	GameKey {0x42, SDL_SCANCODE_F8, u"F8"},
	GameKey {0x43, SDL_SCANCODE_F9, u"F9"},
	GameKey {0x44, SDL_SCANCODE_F10, u"F10"},
	// The system gives the Num Lock key the name of the Pause key, and the Pause key the name of the Num Lock key
	GameKey {0x45, SDL_SCANCODE_NUMLOCKCLEAR, u"Pause"},
	GameKey {0x46, SDL_SCANCODE_SCROLLLOCK, u"Scroll Lock"},
	GameKey {0x47, SDL_SCANCODE_KP_7, u"Num 7"},
	GameKey {0x48, SDL_SCANCODE_KP_8, u"Num 8"},
	GameKey {0x49, SDL_SCANCODE_KP_9, u"Num 9"},
	GameKey {0x4A, SDL_SCANCODE_KP_MINUS, u"Num -"},
	GameKey {0x4B, SDL_SCANCODE_KP_4, u"Num 4"},
	GameKey {0x4C, SDL_SCANCODE_KP_5, u"Num 5"},
	GameKey {0x4D, SDL_SCANCODE_KP_6, u"Num 6"},
	GameKey {0x4E, SDL_SCANCODE_KP_PLUS, u"Num +"},
	GameKey {0x4F, SDL_SCANCODE_KP_1, u"Num 1"},
	GameKey {0x50, SDL_SCANCODE_KP_2, u"Num 2"},
	GameKey {0x51, SDL_SCANCODE_KP_3, u"Num 3"},
	GameKey {0x52, SDL_SCANCODE_KP_0, u"Num 0"},
	GameKey {0x53, SDL_SCANCODE_KP_PERIOD, u"Num Del"},
	GameKey {0x54, SDL_SCANCODE_SYSREQ, u"Sys Req"},
	GameKey {0x56, SDL_SCANCODE_NONUSBACKSLASH, u"\\"},
	GameKey {0x57, SDL_SCANCODE_F11, u"F11"},
	GameKey {0x58, SDL_SCANCODE_F12, u"F12"},
	GameKey {0x9C, SDL_SCANCODE_KP_ENTER, u"Num Enter"},
	GameKey {0x9D, SDL_SCANCODE_RCTRL, u"Ctrl"},
	GameKey {0xB5, SDL_SCANCODE_KP_DIVIDE, u"Num /"},
	GameKey {0xB7, SDL_SCANCODE_PRINTSCREEN, u"Prnt Scrn"},
	GameKey {0xB8, SDL_SCANCODE_RALT, u"Alt"},
	GameKey {0xC5, SDL_SCANCODE_PAUSE, u"Num Lock"},
	GameKey {0xC7, SDL_SCANCODE_HOME, u"Home"},
	GameKey {0xC8, SDL_SCANCODE_UP, u"Up"},
	GameKey {0xC9, SDL_SCANCODE_PAGEUP, u"Page Up"},
	GameKey {0xCB, SDL_SCANCODE_LEFT, u"Left"},
	GameKey {0xCD, SDL_SCANCODE_RIGHT, u"Right"},
	GameKey {0xCF, SDL_SCANCODE_END, u"End"},
	GameKey {0xD0, SDL_SCANCODE_DOWN, u"Down"},
	GameKey {0xD1, SDL_SCANCODE_PAGEDOWN, u"Page Down"},
	GameKey {0xD2, SDL_SCANCODE_INSERT, u"Insert"},
	GameKey {0xD3, SDL_SCANCODE_DELETE, u"Delete"},
	GameKey {0xDB, SDL_SCANCODE_LGUI, u"Left Windows"},
	GameKey {0xDC, SDL_SCANCODE_RGUI, u"Right Windows"},
	GameKey {0xDD, SDL_SCANCODE_APPLICATION, u"Application"},
};
// clang-format on

/// The keys that the game keeps for itself, by their numbers: Escape, Tab, P, F1, F2 and the Enter key of the number pad
constexpr std::array<uint8_t, 6> k_KeptByTheGame {0x01, 0x0F, 0x19, 0x3B, 0x3C, 0x9C};

/// The numbers of the modifier keys, the left key of each, as the profile keeps a modifier
constexpr uint8_t k_ShiftCode = 0x2A;
constexpr uint8_t k_CtrlCode = 0x1D;
constexpr uint8_t k_AltCode = 0x38;

const GameKey* FindKey(SDL_Scancode key)
{
	const auto found = std::ranges::find(k_GameKeys, key, &GameKey::key);
	return found != k_GameKeys.end() ? &*found : nullptr;
}

const GameKey* FindCode(uint32_t code)
{
	const auto found = std::ranges::find(k_GameKeys, code, [](const GameKey& key) { return static_cast<uint32_t>(key.code); });
	return found != k_GameKeys.end() ? &*found : nullptr;
}

uint32_t ModifierCode(Modifier modifier)
{
	switch (modifier)
	{
	case Modifier::Shift:
		return k_ShiftCode;
	case Modifier::Ctrl:
		return k_CtrlCode;
	case Modifier::Alt:
		return k_AltCode;
	case Modifier::None:
		break;
	}
	return 0;
}

/// The modifier of a kept number. Either key of a pair counts as the same modifier.
Modifier ModifierOfCode(uint32_t code)
{
	if (const auto key = KeyOfGameCode(code); key.has_value())
	{
		return ModifierOfKey(*key);
	}
	return Modifier::None;
}

} // namespace

std::optional<uint8_t> GameKeyCode(SDL_Scancode key) noexcept
{
	const auto* found = FindKey(key);
	return found != nullptr ? std::optional(found->code) : std::nullopt;
}

std::optional<SDL_Scancode> KeyOfGameCode(uint32_t code) noexcept
{
	const auto* found = FindCode(code);
	return found != nullptr ? std::optional(found->key) : std::nullopt;
}

std::u16string KeyName(SDL_Scancode key)
{
	const auto* found = FindKey(key);
	return found != nullptr ? std::u16string(found->name) : std::u16string();
}

std::string_view MouseTextName(MouseInput mouse) noexcept
{
	switch (mouse)
	{
	case MouseInput::LeftButton:
		return "HELP_TEXT_ACTIONS_LEFTMOUSEBUTTON";
	case MouseInput::RightButton:
		return "HELP_TEXT_ACTIONS_RIGHTMOUSEBUTTON";
	case MouseInput::MiddleButton:
		return "HELP_TEXT_ACTIONS_MIDDLEMOUSEBUTTON";
	case MouseInput::WheelUp:
		return "HELP_TEXT_ACTIONS_WHEEL_UP";
	case MouseInput::WheelDown:
		return "HELP_TEXT_ACTIONS_WHEEL_DOWN";
	case MouseInput::None:
		break;
	}
	return {};
}

std::u16string ChordName(const std::optional<KeyChord>& chord)
{
	if (!chord.has_value() || chord->key == SDL_SCANCODE_UNKNOWN)
	{
		return {};
	}
	if (chord->modifier == Modifier::None)
	{
		return KeyName(chord->key);
	}
	const auto modifier = KeyOfGameCode(ModifierCode(chord->modifier));
	return KeyName(modifier.value_or(SDL_SCANCODE_UNKNOWN)) + u" + " + KeyName(chord->key);
}

bool IsBindable(SDL_Scancode key) noexcept
{
	const auto code = GameKeyCode(key);
	return code.has_value() && std::ranges::find(k_KeptByTheGame, *code) == k_KeptByTheGame.end();
}

KeyChord CapturedKey(SDL_Scancode key, HeldModifiers held) noexcept
{
	const auto own = ModifierOfKey(key);
	auto modifier = Modifier::None;
	if (held.shift)
	{
		modifier = own == Modifier::Shift ? Modifier::None : Modifier::Shift;
	}
	else if (held.ctrl)
	{
		modifier = own == Modifier::Ctrl ? Modifier::None : Modifier::Ctrl;
	}
	else if (held.alt)
	{
		modifier = own == Modifier::Alt ? Modifier::None : Modifier::Alt;
	}
	return {.key = key, .modifier = modifier};
}

std::optional<MouseInput> CapturedMouse(bool left, bool right, bool middle, int wheelSteps) noexcept
{
	if (left)
	{
		return MouseInput::LeftButton;
	}
	if (right)
	{
		return MouseInput::RightButton;
	}
	if (middle)
	{
		return MouseInput::MiddleButton;
	}
	if (wheelSteps != 0)
	{
		return wheelSteps < 0 ? MouseInput::WheelDown : MouseInput::WheelUp;
	}
	return std::nullopt;
}

void AssignKey(std::span<KeyBinding> bindings, size_t row, std::optional<KeyChord> key)
{
	if (row >= bindings.size())
	{
		return;
	}
	// No key and no modifier is the same as an unbound key
	if (key.has_value() && key->key == SDL_SCANCODE_UNKNOWN)
	{
		key.reset();
	}
	for (size_t i = 0; i < bindings.size(); ++i)
	{
		if (i != row && bindings[i].key == key)
		{
			bindings[i].key.reset();
		}
	}
	bindings[row].key = key;
}

void AssignMouse(std::span<KeyBinding> bindings, size_t row, MouseInput mouse)
{
	if (row >= bindings.size())
	{
		return;
	}
	for (auto& binding : bindings)
	{
		if (binding.mouse == mouse)
		{
			binding.mouse = MouseInput::None;
		}
	}
	bindings[row].mouse = mouse;
}

std::map<uint32_t, player_profiles::KeptKey> ToKept(std::span<const KeyBinding> bindings)
{
	std::map<uint32_t, player_profiles::KeptKey> kept;
	for (size_t i = 0; i < bindings.size(); ++i)
	{
		const auto& binding = bindings[i];
		player_profiles::KeptKey key {.primary = 0, .primaryModifier = 0, .mouseButton = static_cast<uint32_t>(binding.mouse)};
		if (binding.key.has_value())
		{
			key.primary = GameKeyCode(binding.key->key).value_or(0);
			key.primaryModifier = key.primary != 0 ? ModifierCode(binding.key->modifier) : 0;
		}
		kept.emplace(static_cast<uint32_t>(i), key);
	}
	return kept;
}

KeyBindingTable FromKept(const std::map<uint32_t, player_profiles::KeptKey>& kept)
{
	auto bindings = k_DefaultKeyBindings;
	for (const auto& [row, key] : kept)
	{
		if (row >= bindings.size())
		{
			continue;
		}
		auto& binding = bindings.at(row);
		const auto scancode = KeyOfGameCode(key.primary);
		binding.key = scancode.has_value()
		                  ? std::optional(KeyChord {.key = *scancode, .modifier = ModifierOfCode(key.primaryModifier)})
		                  : std::nullopt;
		binding.mouse = key.mouseButton <= static_cast<uint32_t>(MouseInput::RightButton)
		                    ? static_cast<MouseInput>(key.mouseButton)
		                    : MouseInput::None;
	}
	return bindings;
}

} // namespace openblack::input::rebinding
