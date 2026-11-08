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

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL_scancode.h>

#include "BindableActions.h"
#include "Common/Platform.h"

namespace openblack::input
{

/// A modifier key that must be held with a binding's key. Either side's key counts, as in the game.
enum class Modifier : uint8_t
{
	None,
	Ctrl,
	Shift,
	Alt,
};

/// The mouse inputs the game's options screen can bind
enum class MouseInput : uint8_t
{
	None,
	LeftButton,
	MiddleButton,
	WheelUp,
	WheelDown,
	RightButton,
};

/// A key, with a modifier that must be held with it
struct KeyChord
{
	SDL_Scancode key {SDL_SCANCODE_UNKNOWN};
	Modifier modifier {Modifier::None};

	[[nodiscard]] constexpr bool operator==(const KeyChord&) const = default;
};

/// How the options screen's actions are grouped for display
enum class BindCategory : uint8_t
{
	Hand,
	Camera,
	Places,
	Creature,
	Villagers,
	Interface,
	Game,
};

/// Whether an action does something yet
enum class BindStatus : uint8_t
{
	Implemented,
	/// The feature behind the action isn't built yet: pressing it only logs that
	NotYetImplemented,
	/// Handled by the leash work (player-leash branch)
	OwnedByLeashWork,
	/// Handled by the creature mode work (creature-mode branch)
	OwnedByCreatureModeWork,
};

/// One row of the options screen's controls list
struct KeyBinding
{
	BindableActionMap action;
	std::string_view name;
	BindCategory category;
	std::optional<KeyChord> key;
	MouseInput mouse {MouseInput::None};
	BindStatus status {BindStatus::Implemented};

	[[nodiscard]] bool operator==(const KeyBinding&) const = default;
};

inline constexpr size_t k_KeyBindingCount = 33;
using KeyBindingTable = std::array<KeyBinding, k_KeyBindingCount>;

// clang-format off
/// The game's defaults, in the order its options screen lists them. Where the game names the right Ctrl or Shift, it
/// treats either side's key as the same one.
inline constexpr KeyBindingTable k_DefaultKeyBindings {{
	{BindableActionMap::HELP,                   "Help",                        BindCategory::Interface, KeyChord {SDL_SCANCODE_F1},     MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::MOVE,                   "Move",                        BindCategory::Hand,      std::nullopt,                   MouseInput::LeftButton,   BindStatus::Implemented},
	{BindableActionMap::ACTION,                 "Action",                      BindCategory::Hand,      std::nullopt,                   MouseInput::RightButton,  BindStatus::Implemented},
	{BindableActionMap::ZOOM_OUT,               "Zoom Out",                    BindCategory::Camera,    std::nullopt,                   MouseInput::WheelDown,    BindStatus::Implemented},
	{BindableActionMap::ZOOM_IN,                "Zoom In",                     BindCategory::Camera,    std::nullopt,                   MouseInput::WheelUp,      BindStatus::Implemented},
	{BindableActionMap::TALK,                   "Talk",                        BindCategory::Interface, KeyChord {SDL_SCANCODE_T},      MouseInput::None,         BindStatus::NotYetImplemented},
	{BindableActionMap::ZOOM_ON,                "Zoom On",                     BindCategory::Camera,    KeyChord {SDL_SCANCODE_RCTRL},  MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::MOVE_LEFT,              "Move Left",                   BindCategory::Camera,    KeyChord {SDL_SCANCODE_LEFT},   MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::MOVE_RIGHT,             "Move Right",                  BindCategory::Camera,    KeyChord {SDL_SCANCODE_RIGHT},  MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::MOVE_FORWARDS,          "Move Forwards",               BindCategory::Camera,    KeyChord {SDL_SCANCODE_UP},     MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::MOVE_BACKWARDS,         "Move Backwards",              BindCategory::Camera,    KeyChord {SDL_SCANCODE_DOWN},   MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::TILT_UP,                "Tilt Up",                     BindCategory::Camera,    KeyChord {SDL_SCANCODE_A},      MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::TILT_DOWN,              "Tilt Down",                   BindCategory::Camera,    KeyChord {SDL_SCANCODE_Q},      MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::ROTATE_LEFT,            "Rotate Left",                 BindCategory::Camera,    KeyChord {SDL_SCANCODE_Z},      MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::ROTATE_RIGHT,           "Rotate Right",                BindCategory::Camera,    KeyChord {SDL_SCANCODE_U},      MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::ROTATE_ON,              "Tilt/Rotate On",              BindCategory::Camera,    KeyChord {SDL_SCANCODE_RSHIFT}, MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::ROTATE_AROUND_MOUSE_ON, "Tilt/Rotate Around Mouse On", BindCategory::Camera,    std::nullopt,                   MouseInput::MiddleButton, BindStatus::Implemented},
	{BindableActionMap::ZOOM_TO_TEMPLE,         "Zoom To Temple",              BindCategory::Places,    KeyChord {SDL_SCANCODE_SPACE},  MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::ZOOM_TO_CREATURE,       "Zoom To Creature",            BindCategory::Creature,  KeyChord {SDL_SCANCODE_C},      MouseInput::None,         BindStatus::OwnedByCreatureModeWork},
	{BindableActionMap::ZOOM_TO_REALM,          "Zoom To Realm",               BindCategory::Places,    KeyChord {SDL_SCANCODE_F3},     MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::ZOOM_TO_INSIDE_TEMPLE,  "Zoom To Inside Temple",       BindCategory::Places,    KeyChord {SDL_SCANCODE_F4},     MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::ZOOM_TO_CREATURE_ROOM,  "Zoom To Creature Room",       BindCategory::Places,    KeyChord {SDL_SCANCODE_F5},     MouseInput::None,         BindStatus::OwnedByCreatureModeWork},
	{BindableActionMap::ZOOM_TO_CHALLENGE_ROOM, "Zoom To Challenge Room",      BindCategory::Places,    KeyChord {SDL_SCANCODE_F6},     MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::ZOOM_TO_SAVE_GAME_ROOM, "Zoom To Save Game Room",      BindCategory::Places,    KeyChord {SDL_SCANCODE_F7},     MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::ZOOM_TO_OPTIONS_ROOM,   "Zoom To Options Room",        BindCategory::Places,    KeyChord {SDL_SCANCODE_F8},     MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::ZOOM_TO_LIBRARY,        "Zoom To Library",             BindCategory::Places,    KeyChord {SDL_SCANCODE_F9},     MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::LEASH_UNLEASH_CREATURE, "Leash/UnLeash Creature",      BindCategory::Creature,  KeyChord {SDL_SCANCODE_L},      MouseInput::None,         BindStatus::OwnedByLeashWork},
	{BindableActionMap::SHOW_VILLAGER_NAMES,    "Show Villager Names",         BindCategory::Villagers, KeyChord {SDL_SCANCODE_N},      MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::SHOW_VILLAGER_DETAILS,  "Show Villager Details",       BindCategory::Villagers, KeyChord {SDL_SCANCODE_S},      MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::QUICK_SAVE,             "Quick Save",                  BindCategory::Game,      KeyChord {SDL_SCANCODE_S, Modifier::Ctrl}, MouseInput::None, BindStatus::Implemented},
	{BindableActionMap::QUICK_LOAD,             "Quick Load",                  BindCategory::Game,      KeyChord {SDL_SCANCODE_L, Modifier::Ctrl}, MouseInput::None, BindStatus::Implemented},
	{BindableActionMap::PREVIOUS_LEASH,         "Previous Leash",              BindCategory::Creature,  KeyChord {SDL_SCANCODE_V},      MouseInput::None,         BindStatus::OwnedByLeashWork},
	{BindableActionMap::NEXT_LEASH,             "Next Leash",                  BindCategory::Creature,  KeyChord {SDL_SCANCODE_B},      MouseInput::None,         BindStatus::OwnedByLeashWork},
}};
// clang-format on

// clang-format off
/// The defaults of the web build, in the same order. A web browser keeps some keys for its own commands, such as F5 to
/// load the page again and Ctrl+L to go to the address bar. The game cannot stop these commands, so these defaults use
/// other keys for those actions:
/// - Help: H, not F1, which opens the help of the browser.
/// - Zoom To Realm: R, not F3, which finds text on the page.
/// - Zoom To Creature Room: K, not F5, which loads the page again.
/// - Zoom To Challenge Room: J, not F6, which goes to the address bar.
/// - Zoom To Save Game Room: G, not F7, which starts caret browsing.
/// - Zoom To Library: I, not F9, which opens the reader view of some browsers.
/// - Quick Save: Alt+S, not Ctrl+S, which saves the page.
/// - Quick Load: Alt+L, not Ctrl+L, which goes to the address bar.
/// All the other keys are the same as the game's defaults.
inline constexpr KeyBindingTable k_WebDefaultKeyBindings {{
	{BindableActionMap::HELP,                   "Help",                        BindCategory::Interface, KeyChord {SDL_SCANCODE_H},      MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::MOVE,                   "Move",                        BindCategory::Hand,      std::nullopt,                   MouseInput::LeftButton,   BindStatus::Implemented},
	{BindableActionMap::ACTION,                 "Action",                      BindCategory::Hand,      std::nullopt,                   MouseInput::RightButton,  BindStatus::Implemented},
	{BindableActionMap::ZOOM_OUT,               "Zoom Out",                    BindCategory::Camera,    std::nullopt,                   MouseInput::WheelDown,    BindStatus::Implemented},
	{BindableActionMap::ZOOM_IN,                "Zoom In",                     BindCategory::Camera,    std::nullopt,                   MouseInput::WheelUp,      BindStatus::Implemented},
	{BindableActionMap::TALK,                   "Talk",                        BindCategory::Interface, KeyChord {SDL_SCANCODE_T},      MouseInput::None,         BindStatus::NotYetImplemented},
	{BindableActionMap::ZOOM_ON,                "Zoom On",                     BindCategory::Camera,    KeyChord {SDL_SCANCODE_RCTRL},  MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::MOVE_LEFT,              "Move Left",                   BindCategory::Camera,    KeyChord {SDL_SCANCODE_LEFT},   MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::MOVE_RIGHT,             "Move Right",                  BindCategory::Camera,    KeyChord {SDL_SCANCODE_RIGHT},  MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::MOVE_FORWARDS,          "Move Forwards",               BindCategory::Camera,    KeyChord {SDL_SCANCODE_UP},     MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::MOVE_BACKWARDS,         "Move Backwards",              BindCategory::Camera,    KeyChord {SDL_SCANCODE_DOWN},   MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::TILT_UP,                "Tilt Up",                     BindCategory::Camera,    KeyChord {SDL_SCANCODE_A},      MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::TILT_DOWN,              "Tilt Down",                   BindCategory::Camera,    KeyChord {SDL_SCANCODE_Q},      MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::ROTATE_LEFT,            "Rotate Left",                 BindCategory::Camera,    KeyChord {SDL_SCANCODE_Z},      MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::ROTATE_RIGHT,           "Rotate Right",                BindCategory::Camera,    KeyChord {SDL_SCANCODE_U},      MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::ROTATE_ON,              "Tilt/Rotate On",              BindCategory::Camera,    KeyChord {SDL_SCANCODE_RSHIFT}, MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::ROTATE_AROUND_MOUSE_ON, "Tilt/Rotate Around Mouse On", BindCategory::Camera,    std::nullopt,                   MouseInput::MiddleButton, BindStatus::Implemented},
	{BindableActionMap::ZOOM_TO_TEMPLE,         "Zoom To Temple",              BindCategory::Places,    KeyChord {SDL_SCANCODE_SPACE},  MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::ZOOM_TO_CREATURE,       "Zoom To Creature",            BindCategory::Creature,  KeyChord {SDL_SCANCODE_C},      MouseInput::None,         BindStatus::OwnedByCreatureModeWork},
	{BindableActionMap::ZOOM_TO_REALM,          "Zoom To Realm",               BindCategory::Places,    KeyChord {SDL_SCANCODE_R},      MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::ZOOM_TO_INSIDE_TEMPLE,  "Zoom To Inside Temple",       BindCategory::Places,    KeyChord {SDL_SCANCODE_F4},     MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::ZOOM_TO_CREATURE_ROOM,  "Zoom To Creature Room",       BindCategory::Places,    KeyChord {SDL_SCANCODE_K},      MouseInput::None,         BindStatus::OwnedByCreatureModeWork},
	{BindableActionMap::ZOOM_TO_CHALLENGE_ROOM, "Zoom To Challenge Room",      BindCategory::Places,    KeyChord {SDL_SCANCODE_J},      MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::ZOOM_TO_SAVE_GAME_ROOM, "Zoom To Save Game Room",      BindCategory::Places,    KeyChord {SDL_SCANCODE_G},      MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::ZOOM_TO_OPTIONS_ROOM,   "Zoom To Options Room",        BindCategory::Places,    KeyChord {SDL_SCANCODE_F8},     MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::ZOOM_TO_LIBRARY,        "Zoom To Library",             BindCategory::Places,    KeyChord {SDL_SCANCODE_I},      MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::LEASH_UNLEASH_CREATURE, "Leash/UnLeash Creature",      BindCategory::Creature,  KeyChord {SDL_SCANCODE_L},      MouseInput::None,         BindStatus::OwnedByLeashWork},
	{BindableActionMap::SHOW_VILLAGER_NAMES,    "Show Villager Names",         BindCategory::Villagers, KeyChord {SDL_SCANCODE_N},      MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::SHOW_VILLAGER_DETAILS,  "Show Villager Details",       BindCategory::Villagers, KeyChord {SDL_SCANCODE_S},      MouseInput::None,         BindStatus::Implemented},
	{BindableActionMap::QUICK_SAVE,             "Quick Save",                  BindCategory::Game,      KeyChord {SDL_SCANCODE_S, Modifier::Alt} , MouseInput::None, BindStatus::Implemented},
	{BindableActionMap::QUICK_LOAD,             "Quick Load",                  BindCategory::Game,      KeyChord {SDL_SCANCODE_L, Modifier::Alt} , MouseInput::None, BindStatus::Implemented},
	{BindableActionMap::PREVIOUS_LEASH,         "Previous Leash",              BindCategory::Creature,  KeyChord {SDL_SCANCODE_V},      MouseInput::None,         BindStatus::OwnedByLeashWork},
	{BindableActionMap::NEXT_LEASH,             "Next Leash",                  BindCategory::Creature,  KeyChord {SDL_SCANCODE_B},      MouseInput::None,         BindStatus::OwnedByLeashWork},
}};
// clang-format on

/// The defaults for a mouse that has no wheel. Zoom In and Zoom Out get no mouse input, and Tilt/Rotate Around Mouse On
/// gets the 0 key in place of the middle button. All the other rows stay the same.
[[nodiscard]] constexpr KeyBindingTable WithoutWheel(KeyBindingTable bindings) noexcept
{
	for (auto& binding : bindings)
	{
		if (binding.action == BindableActionMap::ZOOM_OUT || binding.action == BindableActionMap::ZOOM_IN)
		{
			binding.mouse = MouseInput::None;
		}
		else if (binding.action == BindableActionMap::ROTATE_AROUND_MOUSE_ON)
		{
			binding.key = KeyChord {SDL_SCANCODE_0};
			binding.mouse = MouseInput::None;
		}
	}
	return bindings;
}

inline constexpr KeyBindingTable k_NoWheelDefaultKeyBindings = WithoutWheel(k_DefaultKeyBindings);
inline constexpr KeyBindingTable k_WebNoWheelDefaultKeyBindings = WithoutWheel(k_WebDefaultKeyBindings);

/// The default key bindings of the platform: the game's defaults on the desktop, and the web defaults in a web browser.
/// When the mouse has no wheel, the defaults for a mouse with no wheel apply.
[[nodiscard]] constexpr const KeyBindingTable& DefaultKeyBindings(Platform platform = k_Platform,
                                                                  bool mouseHasWheel = true) noexcept
{
	if (platform == Platform::Web)
	{
		return mouseHasWheel ? k_WebDefaultKeyBindings : k_WebNoWheelDefaultKeyBindings;
	}
	return mouseHasWheel ? k_DefaultKeyBindings : k_NoWheelDefaultKeyBindings;
}

/// Whether a press of an action does the work of the action. Inside the temple, the temple takes the presses of the
/// keyboard and uses them only for the keys of its rooms. A press of a mouse button that has the action always does
/// the work.
[[nodiscard]] constexpr bool PressDoesAction(bool inTemple, bool byMouse) noexcept
{
	return !inTemple || byMouse;
}

/// The key that stands for both sides' modifier keys: the left one for either Ctrl, Shift or Alt
[[nodiscard]] SDL_Scancode CanonicalKey(SDL_Scancode key) noexcept;
/// Whether SDL's modifier state holds the modifier, on either side
[[nodiscard]] bool ModifierHeld(Modifier modifier, uint16_t sdlModifiers) noexcept;
/// The modifier a modifier key itself is, or none for any other key
[[nodiscard]] Modifier ModifierOfKey(SDL_Scancode key) noexcept;

/// The actions a key press starts. A binding that needs a held modifier wins over one on the same key without it, so
/// Ctrl+S quick saves rather than also showing villagers' details.
[[nodiscard]] BindableActionMap ActionsForKeyDown(std::span<const KeyBinding> bindings, SDL_Scancode key,
                                                  uint16_t sdlModifiers) noexcept;
/// What an action bound to a pressed key does to the game's other keys
enum class KeyDownBinding : uint8_t
{
	/// No action takes the key, so the speed keys and the other keys of the game act on it
	None,
	/// An action that acts while it is held takes the key. The speed keys do not act, but the other keys do.
	LetsKeyThrough,
	/// The action takes the key and no other key of the game acts on it
	TakesKey,
};

/// Whether the action acts while it is held: the hand and camera actions from moving the hand to turning around the
/// mouse, but talking. The other actions act once, when their key goes down.
[[nodiscard]] bool IsHeldAction(BindableActionMap action) noexcept;
/// The actions of the set that act once, when their key or mouse button goes down
[[nodiscard]] BindableActionMap OnePressActions(BindableActionMap actions) noexcept;
/// The actions that a key press holds. An action that acts while it is held is held with its key, and with its
/// modifier when it has one; other modifiers held do not stop it. A one-press action is held only when it is the row
/// that the press finds (BindingForKeyDown).
[[nodiscard]] BindableActionMap ActionsPressedByKeyDown(std::span<const KeyBinding> bindings, SDL_Scancode key,
                                                        uint16_t sdlModifiers, BindableActionMap blocked) noexcept;
/// The one modifier that a key press has, as the game finds it: Shift, then Ctrl, then Alt
[[nodiscard]] Modifier KeyDownModifier(uint16_t sdlModifiers) noexcept;
/// The first row whose key and modifier are those of the key press. A plain binding needs no modifier held, and a
/// binding with a modifier needs that modifier as the press's one modifier. None when no row has them, or when the
/// first such row is in `blocked`.
[[nodiscard]] std::optional<size_t> BindingForKeyDown(std::span<const KeyBinding> bindings, SDL_Scancode key,
                                                      uint16_t sdlModifiers, BindableActionMap blocked) noexcept;
/// What the action of a key press does to the game's other keys. The hand and camera actions that act while they are
/// held let the key through, and so does every action while a script holds the screen.
[[nodiscard]] KeyDownBinding KeyDownBindingFor(std::span<const KeyBinding> bindings, SDL_Scancode key, uint16_t sdlModifiers,
                                               BindableActionMap blocked, bool scriptHoldsScreen) noexcept;
/// Every action bound to the key, with or without a modifier, which letting go of it ends
[[nodiscard]] BindableActionMap ActionsForKey(std::span<const KeyBinding> bindings, SDL_Scancode key) noexcept;
/// The actions bound to a mouse input
[[nodiscard]] BindableActionMap ActionsForMouse(std::span<const KeyBinding> bindings, MouseInput mouse) noexcept;

/// Where an action is in the table
[[nodiscard]] std::optional<size_t> IndexOf(std::span<const KeyBinding> bindings, BindableActionMap action) noexcept;

/// Pairs of rows bound to the same key chord or the same mouse input
[[nodiscard]] std::vector<std::pair<size_t, size_t>> FindConflicts(std::span<const KeyBinding> bindings);

/// How the options screen would show a key chord, such as "Ctrl+S"
[[nodiscard]] std::string KeyChordName(const KeyChord& chord);
[[nodiscard]] std::string_view MouseInputName(MouseInput mouse) noexcept;
[[nodiscard]] std::string_view CategoryName(BindCategory category) noexcept;
[[nodiscard]] std::string_view StatusName(BindStatus status) noexcept;

} // namespace openblack::input
