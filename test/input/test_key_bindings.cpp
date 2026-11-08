/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <numbers>
#include <optional>
#include <string_view>

#include <SDL_events.h>
#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Input/GameActionMap.h"
#include "Input/KeyBindings.h"

using namespace openblack;
using namespace openblack::input;

namespace
{
struct ExpectedBinding
{
	BindableActionMap action;
	std::string_view name;
	SDL_Scancode key;
	Modifier modifier;
	MouseInput mouse;
};

// The options screen's controls list as the game sets it up, in its order
// clang-format off
constexpr std::array<ExpectedBinding, k_KeyBindingCount> k_Documented {{
	{BindableActionMap::HELP,                   "Help",                        SDL_SCANCODE_F1,      Modifier::None, MouseInput::None},
	{BindableActionMap::MOVE,                   "Move",                        SDL_SCANCODE_UNKNOWN, Modifier::None, MouseInput::LeftButton},
	{BindableActionMap::ACTION,                 "Action",                      SDL_SCANCODE_UNKNOWN, Modifier::None, MouseInput::RightButton},
	{BindableActionMap::ZOOM_OUT,               "Zoom Out",                    SDL_SCANCODE_UNKNOWN, Modifier::None, MouseInput::WheelDown},
	{BindableActionMap::ZOOM_IN,                "Zoom In",                     SDL_SCANCODE_UNKNOWN, Modifier::None, MouseInput::WheelUp},
	{BindableActionMap::TALK,                   "Talk",                        SDL_SCANCODE_T,       Modifier::None, MouseInput::None},
	{BindableActionMap::ZOOM_ON,                "Zoom On",                     SDL_SCANCODE_RCTRL,   Modifier::None, MouseInput::None},
	{BindableActionMap::MOVE_LEFT,              "Move Left",                   SDL_SCANCODE_LEFT,    Modifier::None, MouseInput::None},
	{BindableActionMap::MOVE_RIGHT,             "Move Right",                  SDL_SCANCODE_RIGHT,   Modifier::None, MouseInput::None},
	{BindableActionMap::MOVE_FORWARDS,          "Move Forwards",               SDL_SCANCODE_UP,      Modifier::None, MouseInput::None},
	{BindableActionMap::MOVE_BACKWARDS,         "Move Backwards",              SDL_SCANCODE_DOWN,    Modifier::None, MouseInput::None},
	{BindableActionMap::TILT_UP,                "Tilt Up",                     SDL_SCANCODE_A,       Modifier::None, MouseInput::None},
	{BindableActionMap::TILT_DOWN,              "Tilt Down",                   SDL_SCANCODE_Q,       Modifier::None, MouseInput::None},
	{BindableActionMap::ROTATE_LEFT,            "Rotate Left",                 SDL_SCANCODE_Z,       Modifier::None, MouseInput::None},
	{BindableActionMap::ROTATE_RIGHT,           "Rotate Right",                SDL_SCANCODE_U,       Modifier::None, MouseInput::None},
	{BindableActionMap::ROTATE_ON,              "Tilt/Rotate On",              SDL_SCANCODE_RSHIFT,  Modifier::None, MouseInput::None},
	{BindableActionMap::ROTATE_AROUND_MOUSE_ON, "Tilt/Rotate Around Mouse On", SDL_SCANCODE_UNKNOWN, Modifier::None, MouseInput::MiddleButton},
	{BindableActionMap::ZOOM_TO_TEMPLE,         "Zoom To Temple",              SDL_SCANCODE_SPACE,   Modifier::None, MouseInput::None},
	{BindableActionMap::ZOOM_TO_CREATURE,       "Zoom To Creature",            SDL_SCANCODE_C,       Modifier::None, MouseInput::None},
	{BindableActionMap::ZOOM_TO_REALM,          "Zoom To Realm",               SDL_SCANCODE_F3,      Modifier::None, MouseInput::None},
	{BindableActionMap::ZOOM_TO_INSIDE_TEMPLE,  "Zoom To Inside Temple",       SDL_SCANCODE_F4,      Modifier::None, MouseInput::None},
	{BindableActionMap::ZOOM_TO_CREATURE_ROOM,  "Zoom To Creature Room",       SDL_SCANCODE_F5,      Modifier::None, MouseInput::None},
	{BindableActionMap::ZOOM_TO_CHALLENGE_ROOM, "Zoom To Challenge Room",      SDL_SCANCODE_F6,      Modifier::None, MouseInput::None},
	{BindableActionMap::ZOOM_TO_SAVE_GAME_ROOM, "Zoom To Save Game Room",      SDL_SCANCODE_F7,      Modifier::None, MouseInput::None},
	{BindableActionMap::ZOOM_TO_OPTIONS_ROOM,   "Zoom To Options Room",        SDL_SCANCODE_F8,      Modifier::None, MouseInput::None},
	{BindableActionMap::ZOOM_TO_LIBRARY,        "Zoom To Library",             SDL_SCANCODE_F9,      Modifier::None, MouseInput::None},
	{BindableActionMap::LEASH_UNLEASH_CREATURE, "Leash/UnLeash Creature",      SDL_SCANCODE_L,       Modifier::None, MouseInput::None},
	{BindableActionMap::SHOW_VILLAGER_NAMES,    "Show Villager Names",         SDL_SCANCODE_N,       Modifier::None, MouseInput::None},
	{BindableActionMap::SHOW_VILLAGER_DETAILS,  "Show Villager Details",       SDL_SCANCODE_S,       Modifier::None, MouseInput::None},
	{BindableActionMap::QUICK_SAVE,             "Quick Save",                  SDL_SCANCODE_S,       Modifier::Ctrl, MouseInput::None},
	{BindableActionMap::QUICK_LOAD,             "Quick Load",                  SDL_SCANCODE_L,       Modifier::Ctrl, MouseInput::None},
	{BindableActionMap::PREVIOUS_LEASH,         "Previous Leash",              SDL_SCANCODE_V,       Modifier::None, MouseInput::None},
	{BindableActionMap::NEXT_LEASH,             "Next Leash",                  SDL_SCANCODE_B,       Modifier::None, MouseInput::None},
}};
// clang-format on

[[nodiscard]] SDL_Event KeyEvent(uint32_t type, SDL_Scancode key, uint16_t modifiers = KMOD_NONE)
{
	SDL_Event event {};
	event.type = type;
	event.key.keysym.scancode = key;
	event.key.keysym.mod = modifiers;
	return event;
}

[[nodiscard]] bool Pressed(const GameActionMap& map, BindableActionMap action)
{
	return map.Get(action) && map.GetChanged(action);
}
} // namespace

TEST(KeyBindings, DefaultsMatchTheOptionsScreen)
{
	for (size_t i = 0; i < k_KeyBindingCount; ++i)
	{
		const auto& binding = k_DefaultKeyBindings.at(i);
		const auto& expected = k_Documented.at(i);
		SCOPED_TRACE(expected.name);
		EXPECT_EQ(binding.action, expected.action);
		EXPECT_EQ(binding.name, expected.name);
		EXPECT_EQ(binding.mouse, expected.mouse);
		if (expected.key == SDL_SCANCODE_UNKNOWN)
		{
			EXPECT_FALSE(binding.key.has_value());
		}
		else
		{
			ASSERT_TRUE(binding.key.has_value());
			EXPECT_EQ(binding.key->key, expected.key);
			EXPECT_EQ(binding.key->modifier, expected.modifier);
		}
	}
}

TEST(KeyBindings, EveryActionIsInTheTableOnce)
{
	uint64_t seen = 0;
	for (const auto& binding : k_DefaultKeyBindings)
	{
		const auto bit = static_cast<uint64_t>(binding.action);
		EXPECT_EQ(seen & bit, 0u) << binding.name;
		seen |= bit;
	}
	EXPECT_EQ(seen, static_cast<uint64_t>(BindableActionMap::ALL));
}

TEST(KeyBindings, DefaultsHaveNoConflicts)
{
	EXPECT_TRUE(FindConflicts(k_DefaultKeyBindings).empty());
}

TEST(KeyBindings, ConflictsAreFound)
{
	auto bindings = k_DefaultKeyBindings;
	bindings.at(*IndexOf(bindings, BindableActionMap::TALK)).key = KeyChord {SDL_SCANCODE_N};
	const auto conflicts = FindConflicts(bindings);
	ASSERT_EQ(conflicts.size(), 1u);
	EXPECT_EQ(bindings.at(conflicts[0].first).action, BindableActionMap::TALK);
	EXPECT_EQ(bindings.at(conflicts[0].second).action, BindableActionMap::SHOW_VILLAGER_NAMES);
}

TEST(KeyBindings, KeysMapToActions)
{
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_SPACE, KMOD_NONE), BindableActionMap::ZOOM_TO_TEMPLE);
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_F3, KMOD_NONE), BindableActionMap::ZOOM_TO_REALM);
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_X, KMOD_NONE), BindableActionMap::NONE);
}

TEST(KeyBindings, EitherSidesModifierKeyCounts)
{
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_LCTRL, KMOD_LCTRL), BindableActionMap::ZOOM_ON);
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_RCTRL, KMOD_RCTRL), BindableActionMap::ZOOM_ON);
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_LSHIFT, KMOD_LSHIFT), BindableActionMap::ROTATE_ON);
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_RSHIFT, KMOD_RSHIFT), BindableActionMap::ROTATE_ON);
}

TEST(KeyBindings, ModifierBindingWinsOverPlainOne)
{
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_S, KMOD_NONE), BindableActionMap::SHOW_VILLAGER_DETAILS);
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_S, KMOD_RCTRL), BindableActionMap::QUICK_SAVE);
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_L, KMOD_NONE), BindableActionMap::LEASH_UNLEASH_CREATURE);
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_L, KMOD_LCTRL), BindableActionMap::QUICK_LOAD);
	// Shift isn't the quick save's modifier, so S with Shift is still the details
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_S, KMOD_LSHIFT), BindableActionMap::SHOW_VILLAGER_DETAILS);
}

TEST(KeyBindings, LettingGoOfAKeyEndsAllItsActions)
{
	EXPECT_EQ(ActionsForKey(k_DefaultKeyBindings, SDL_SCANCODE_S),
	          static_cast<BindableActionMap>(static_cast<uint64_t>(BindableActionMap::SHOW_VILLAGER_DETAILS) |
	                                         static_cast<uint64_t>(BindableActionMap::QUICK_SAVE)));
}

TEST(KeyBindings, MouseMapsToActions)
{
	EXPECT_EQ(ActionsForMouse(k_DefaultKeyBindings, MouseInput::LeftButton), BindableActionMap::MOVE);
	EXPECT_EQ(ActionsForMouse(k_DefaultKeyBindings, MouseInput::RightButton), BindableActionMap::ACTION);
	EXPECT_EQ(ActionsForMouse(k_DefaultKeyBindings, MouseInput::MiddleButton), BindableActionMap::ROTATE_AROUND_MOUSE_ON);
	EXPECT_EQ(ActionsForMouse(k_DefaultKeyBindings, MouseInput::WheelUp), BindableActionMap::ZOOM_IN);
	EXPECT_EQ(ActionsForMouse(k_DefaultKeyBindings, MouseInput::WheelDown), BindableActionMap::ZOOM_OUT);
	EXPECT_EQ(ActionsForMouse(k_DefaultKeyBindings, MouseInput::None), BindableActionMap::NONE);
}

TEST(KeyBindings, Names)
{
	EXPECT_EQ(KeyChordName({SDL_SCANCODE_S, Modifier::Ctrl}), "Ctrl+S");
	EXPECT_EQ(KeyChordName({SDL_SCANCODE_RCTRL}), "Ctrl");
	EXPECT_EQ(KeyChordName({SDL_SCANCODE_F3}), "F3");
}

TEST(GameActionMap, KeyEventsPressAndReleaseActions)
{
	GameActionMap map;
	map.Frame();
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_N));
	EXPECT_TRUE(Pressed(map, BindableActionMap::SHOW_VILLAGER_NAMES));
	map.Frame();
	EXPECT_TRUE(map.Get(BindableActionMap::SHOW_VILLAGER_NAMES));
	EXPECT_FALSE(map.GetChanged(BindableActionMap::SHOW_VILLAGER_NAMES));
	map.ProcessEvent(KeyEvent(SDL_KEYUP, SDL_SCANCODE_N));
	EXPECT_FALSE(map.Get(BindableActionMap::SHOW_VILLAGER_NAMES));
}

TEST(GameActionMap, ChordReplacesThePlainKey)
{
	GameActionMap map;
	map.Frame();
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_LCTRL, KMOD_LCTRL));
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_S, KMOD_LCTRL));
	EXPECT_TRUE(map.Get(BindableActionMap::ZOOM_ON));
	EXPECT_TRUE(Pressed(map, BindableActionMap::QUICK_SAVE));
	EXPECT_FALSE(map.Get(BindableActionMap::SHOW_VILLAGER_DETAILS));
	map.ProcessEvent(KeyEvent(SDL_KEYUP, SDL_SCANCODE_S, KMOD_LCTRL));
	// The press stays for the frame in which the key went down, and the next frame lets go of it
	EXPECT_TRUE(map.Get(BindableActionMap::QUICK_SAVE));
	map.Frame();
	EXPECT_FALSE(map.Get(BindableActionMap::QUICK_SAVE));
}

// The game keeps each key press until its frame reads it. A key that goes down and up within one frame shows as pressed
// for that frame, and as let go in the next frame.
TEST(GameActionMap, AKeyPressedAndLetGoInOneFrameIsNotLost)
{
	GameActionMap map;
	map.Frame();
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_F5));
	map.ProcessEvent(KeyEvent(SDL_KEYUP, SDL_SCANCODE_F5));
	EXPECT_TRUE(Pressed(map, BindableActionMap::ZOOM_TO_CREATURE_ROOM));
	map.Frame();
	EXPECT_FALSE(map.Get(BindableActionMap::ZOOM_TO_CREATURE_ROOM));
	EXPECT_TRUE(map.GetChanged(BindableActionMap::ZOOM_TO_CREATURE_ROOM));
	map.Frame();
	EXPECT_FALSE(map.GetChanged(BindableActionMap::ZOOM_TO_CREATURE_ROOM));
}

// A key that goes down, up and down again in one frame stays held
TEST(GameActionMap, AKeyPressedAgainInTheSameFrameStaysHeld)
{
	GameActionMap map;
	map.Frame();
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_LEFT));
	map.ProcessEvent(KeyEvent(SDL_KEYUP, SDL_SCANCODE_LEFT));
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_LEFT));
	map.Frame();
	EXPECT_TRUE(map.Get(BindableActionMap::MOVE_LEFT));
	map.ProcessEvent(KeyEvent(SDL_KEYUP, SDL_SCANCODE_LEFT));
	EXPECT_FALSE(map.Get(BindableActionMap::MOVE_LEFT));
}

TEST(GameActionMap, RebindingMovesTheAction)
{
	GameActionMap map;
	map.SetKeyBinding(BindableActionMap::ZOOM_TO_TEMPLE, KeyChord {SDL_SCANCODE_HOME});
	map.Frame();
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE));
	EXPECT_FALSE(map.Get(BindableActionMap::ZOOM_TO_TEMPLE));
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_HOME));
	EXPECT_TRUE(Pressed(map, BindableActionMap::ZOOM_TO_TEMPLE));
	map.ResetKeyBindings();
	EXPECT_EQ(map.GetKeyBindings()[*IndexOf(map.GetKeyBindings(), BindableActionMap::ZOOM_TO_TEMPLE)].key,
	          KeyChord {SDL_SCANCODE_SPACE});
}

TEST(GameActionMap, QueuedPressGoesThroughTheKeyPath)
{
	GameActionMap map;
	map.Frame();
	// Every action, pressed from the debug window, shows as pressed for one frame and let go of the next
	for (const auto& binding : k_DefaultKeyBindings)
	{
		SCOPED_TRACE(binding.name);
		map.QueuePress(binding.action);
		EXPECT_TRUE(map.HasQueuedPresses());
		map.Frame();
		EXPECT_TRUE(Pressed(map, binding.action));
		map.Frame();
		EXPECT_FALSE(map.Get(binding.action));
		map.Frame();
		EXPECT_FALSE(map.HasQueuedPresses());
	}
}

TEST(GameActionMap, QueuedChordPressesOnlyTheChordsAction)
{
	GameActionMap map;
	map.Frame();
	map.QueuePress(BindableActionMap::QUICK_SAVE);
	map.Frame();
	EXPECT_TRUE(Pressed(map, BindableActionMap::QUICK_SAVE));
	EXPECT_FALSE(map.Get(BindableActionMap::SHOW_VILLAGER_DETAILS));
}

TEST(GameActionMap, HeldKeyReadsAsNotHeldWhileABoxHasTheKeyboard)
{
	GameActionMap map;
	map.Frame();
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_UP));
	map.Frame();
	ASSERT_TRUE(map.Get(BindableActionMap::MOVE_FORWARDS));
	// A box opens and a key goes down: the box takes the keyboard, and the held key reads as not held
	map.BoxKeyDown(true);
	EXPECT_TRUE(map.BoxHasKeyboard());
	EXPECT_FALSE(map.Get(BindableActionMap::MOVE_FORWARDS));
	EXPECT_FALSE(map.GetRepeat(BindableActionMap::MOVE_FORWARDS));
	EXPECT_FALSE(map.GetChanged(BindableActionMap::MOVE_FORWARDS));
	map.Frame();
	EXPECT_FALSE(map.Get(BindableActionMap::MOVE_FORWARDS));
	// The box closed and the next key goes down: the key that is still held reads as held again, but not as a new press
	map.BoxKeyDown(false);
	EXPECT_TRUE(map.Get(BindableActionMap::MOVE_FORWARDS));
	EXPECT_FALSE(map.GetChanged(BindableActionMap::MOVE_FORWARDS));
}

TEST(GameActionMap, ABoxWithTheKeyboardHidesTheMouseActionsToo)
{
	GameActionMap map;
	map.Frame();
	SDL_Event press {};
	press.type = SDL_MOUSEBUTTONDOWN;
	press.button.button = SDL_BUTTON_MIDDLE;
	map.ProcessEvent(press);
	ASSERT_TRUE(map.Get(BindableActionMap::ROTATE_AROUND_MOUSE_ON));
	map.BoxKeyDown(true);
	EXPECT_FALSE(map.Get(BindableActionMap::ROTATE_AROUND_MOUSE_ON));
	EXPECT_FALSE(map.GetAny(BindableActionMap::ROTATE_AROUND_MOUSE_ON, BindableActionMap::MOVE));
}

TEST(GameActionMap, AKeyThatComesUpUnderTheBoxIsNotHeldAfterIt)
{
	GameActionMap map;
	map.Frame();
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_UP));
	map.Frame();
	map.BoxKeyDown(true);
	map.ProcessEvent(KeyEvent(SDL_KEYUP, SDL_SCANCODE_UP));
	map.Frame();
	map.BoxKeyDown(false);
	EXPECT_FALSE(map.Get(BindableActionMap::MOVE_FORWARDS));
	EXPECT_FALSE(map.GetChanged(BindableActionMap::MOVE_FORWARDS));
}

TEST(GameActionMap, AnAnsweredMessageBoxGivesTheKeyboardBackAtOnce)
{
	GameActionMap map;
	map.Frame();
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_UP));
	map.Frame();
	map.BoxKeyDown(true);
	ASSERT_TRUE(map.BoxHasKeyboard());
	// The player answers the box with the mouse: no key goes down, and the held key reads as held again
	map.BoxAnswered();
	EXPECT_FALSE(map.BoxHasKeyboard());
	EXPECT_TRUE(map.Get(BindableActionMap::MOVE_FORWARDS));
}

// With a mouse that has no wheel, the zoom rows have no mouse input and the turn around the cursor is on the 0 key
TEST(KeyBindings, NoWheelDefaultsMatchTheGame)
{
	for (size_t i = 0; i < k_KeyBindingCount; ++i)
	{
		const auto& withWheel = k_DefaultKeyBindings.at(i);
		const auto& noWheel = k_NoWheelDefaultKeyBindings.at(i);
		EXPECT_EQ(noWheel.action, withWheel.action) << i;
		EXPECT_EQ(noWheel.name, withWheel.name) << i;
		switch (withWheel.action)
		{
		case BindableActionMap::ZOOM_OUT:
		case BindableActionMap::ZOOM_IN:
			EXPECT_EQ(noWheel.key, std::nullopt) << i;
			EXPECT_EQ(noWheel.mouse, MouseInput::None) << i;
			break;
		case BindableActionMap::ROTATE_AROUND_MOUSE_ON:
			EXPECT_EQ(noWheel.key, KeyChord {SDL_SCANCODE_0}) << i;
			EXPECT_EQ(noWheel.mouse, MouseInput::None) << i;
			break;
		default:
			EXPECT_EQ(noWheel, withWheel) << i;
			break;
		}
	}
	EXPECT_TRUE(FindConflicts(k_NoWheelDefaultKeyBindings).empty());
	EXPECT_EQ(ActionsForKeyDown(k_NoWheelDefaultKeyBindings, SDL_SCANCODE_0, KMOD_NONE),
	          BindableActionMap::ROTATE_AROUND_MOUSE_ON);
	EXPECT_EQ(ActionsForMouse(k_NoWheelDefaultKeyBindings, MouseInput::MiddleButton), BindableActionMap::NONE);
}

TEST(KeyBindings, DefaultsFollowTheWheel)
{
	EXPECT_EQ(&DefaultKeyBindings(Platform::Desktop, true), &k_DefaultKeyBindings);
	EXPECT_EQ(&DefaultKeyBindings(Platform::Desktop, false), &k_NoWheelDefaultKeyBindings);
	EXPECT_EQ(&DefaultKeyBindings(Platform::Web, true), &k_WebDefaultKeyBindings);
	EXPECT_EQ(&DefaultKeyBindings(Platform::Web, false), &k_WebNoWheelDefaultKeyBindings);
	EXPECT_EQ(ActionsForKeyDown(k_WebNoWheelDefaultKeyBindings, SDL_SCANCODE_0, KMOD_NONE),
	          BindableActionMap::ROTATE_AROUND_MOUSE_ON);
}

// The game starts with the defaults for a mouse with no wheel when the system says so. After the wheel turns, Load
// Defaults gives the defaults for a mouse with a wheel.
TEST(GameActionMap, NoWheelDefaultsUntilTheWheelTurns)
{
	GameActionMap map(false);
	EXPECT_EQ(map.GetKeyBindings()[*IndexOf(map.GetKeyBindings(), BindableActionMap::ROTATE_AROUND_MOUSE_ON)].key,
	          KeyChord {SDL_SCANCODE_0});
	EXPECT_EQ(&map.GetDefaultKeyBindings(), &DefaultKeyBindings(k_Platform, false));
	map.Frame();
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_0));
	EXPECT_TRUE(Pressed(map, BindableActionMap::ROTATE_AROUND_MOUSE_ON));
	map.ProcessEvent(KeyEvent(SDL_KEYUP, SDL_SCANCODE_0));

	SDL_Event wheel {};
	wheel.type = SDL_MOUSEWHEEL;
	wheel.wheel.y = 1;
	wheel.wheel.preciseY = 1.0f;
	map.ProcessEvent(wheel);
	EXPECT_EQ(&map.GetDefaultKeyBindings(), &DefaultKeyBindings(k_Platform, true));
	map.ResetKeyBindings();
	EXPECT_EQ(map.GetKeyBindings()[*IndexOf(map.GetKeyBindings(), BindableActionMap::ROTATE_AROUND_MOUSE_ON)].mouse,
	          MouseInput::MiddleButton);
}

TEST(GameActionMap, WheelDefaultsWhenTheSystemSaysWheel)
{
	const GameActionMap map;
	EXPECT_EQ(&map.GetDefaultKeyBindings(), &DefaultKeyBindings());
	EXPECT_TRUE(std::ranges::equal(map.GetKeyBindings(), DefaultKeyBindings()));
}

TEST(KeyBindings, KeyDownTakesOneModifierShiftThenCtrlThenAlt)
{
	EXPECT_EQ(KeyDownModifier(KMOD_NONE), Modifier::None);
	EXPECT_EQ(KeyDownModifier(KMOD_LALT | KMOD_RCTRL | KMOD_LSHIFT), Modifier::Shift);
	EXPECT_EQ(KeyDownModifier(KMOD_LALT | KMOD_LCTRL), Modifier::Ctrl);
	EXPECT_EQ(KeyDownModifier(KMOD_RALT), Modifier::Alt);
}

TEST(KeyBindings, KeyDownFindsTheFirstRowWithTheKeyAndItsModifier)
{
	const auto none = BindableActionMap::NONE;
	// S alone is the details, Ctrl+S the quick save, and Shift+S neither
	EXPECT_EQ(BindingForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_S, KMOD_NONE, none),
	          IndexOf(k_DefaultKeyBindings, BindableActionMap::SHOW_VILLAGER_DETAILS));
	EXPECT_EQ(BindingForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_S, KMOD_RCTRL, none),
	          IndexOf(k_DefaultKeyBindings, BindableActionMap::QUICK_SAVE));
	EXPECT_EQ(BindingForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_S, KMOD_LSHIFT, none), std::nullopt);
	// With Shift and Ctrl held, the press's modifier is Shift, so Ctrl+S does not match
	EXPECT_EQ(BindingForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_S, KMOD_LSHIFT | KMOD_LCTRL, none), std::nullopt);
	// Either side's key is the same key
	EXPECT_EQ(BindingForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_LSHIFT, KMOD_NONE, none),
	          IndexOf(k_DefaultKeyBindings, BindableActionMap::ROTATE_ON));
	EXPECT_EQ(BindingForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_X, KMOD_NONE, none), std::nullopt);
}

TEST(KeyBindings, KeyDownStopsAtTheFirstRowWhenItIsBlocked)
{
	auto bindings = k_DefaultKeyBindings;
	// Two rows on the same key: the temple's row is first
	bindings.at(*IndexOf(bindings, BindableActionMap::ZOOM_TO_TEMPLE)).key = KeyChord {SDL_SCANCODE_1};
	bindings.at(*IndexOf(bindings, BindableActionMap::SHOW_VILLAGER_NAMES)).key = KeyChord {SDL_SCANCODE_1};
	EXPECT_EQ(BindingForKeyDown(bindings, SDL_SCANCODE_1, KMOD_NONE, BindableActionMap::NONE),
	          IndexOf(bindings, BindableActionMap::ZOOM_TO_TEMPLE));
	// The help blocks the first row, and the key then has no action, also not the second row
	EXPECT_EQ(BindingForKeyDown(bindings, SDL_SCANCODE_1, KMOD_NONE, BindableActionMap::ZOOM_TO_TEMPLE), std::nullopt);
}

TEST(KeyBindings, KeyDownBindingTellsWhichOtherKeysAct)
{
	auto bindings = k_DefaultKeyBindings;
	bindings.at(*IndexOf(bindings, BindableActionMap::HELP)).key = KeyChord {SDL_SCANCODE_1};
	bindings.at(*IndexOf(bindings, BindableActionMap::MOVE_LEFT)).key = KeyChord {SDL_SCANCODE_2, Modifier::Alt};
	const auto none = BindableActionMap::NONE;
	// A one-press action takes the key
	EXPECT_EQ(KeyDownBindingFor(bindings, SDL_SCANCODE_1, KMOD_NONE, none, false), KeyDownBinding::TakesKey);
	// The same action while a script holds the screen lets the key through
	EXPECT_EQ(KeyDownBindingFor(bindings, SDL_SCANCODE_1, KMOD_NONE, none, true), KeyDownBinding::LetsKeyThrough);
	// A held camera action lets the key through
	EXPECT_EQ(KeyDownBindingFor(bindings, SDL_SCANCODE_2, KMOD_LALT, none, false), KeyDownBinding::LetsKeyThrough);
	// A blocked action, or no action, leaves the key to the other keys
	EXPECT_EQ(KeyDownBindingFor(bindings, SDL_SCANCODE_2, KMOD_LALT, BindableActionMap::MOVE_LEFT, false),
	          KeyDownBinding::None);
	EXPECT_EQ(KeyDownBindingFor(bindings, SDL_SCANCODE_1, KMOD_LSHIFT, none, false), KeyDownBinding::None);
	// Talking is not a held action
	EXPECT_EQ(KeyDownBindingFor(k_DefaultKeyBindings, SDL_SCANCODE_T, KMOD_NONE, none, false), KeyDownBinding::TakesKey);
}

TEST(KeyBindings, OnePressActionsNeedTheKeyWithItsOneModifier)
{
	const auto none = BindableActionMap::NONE;
	// S with Shift held is neither the details nor the quick save
	EXPECT_EQ(ActionsPressedByKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_S, KMOD_LSHIFT, none), none);
	EXPECT_EQ(ActionsPressedByKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_S, KMOD_NONE, none),
	          BindableActionMap::SHOW_VILLAGER_DETAILS);
	EXPECT_EQ(ActionsPressedByKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_S, KMOD_LCTRL, none), BindableActionMap::QUICK_SAVE);
	// The help blocks the temple's key
	EXPECT_EQ(ActionsPressedByKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_SPACE, KMOD_NONE, BindableActionMap::ZOOM_TO_TEMPLE),
	          none);
}

TEST(KeyBindings, HeldActionsStayHeldWithOtherModifiers)
{
	const auto none = BindableActionMap::NONE;
	// The camera's keys move it with Shift or Alt held too
	EXPECT_EQ(ActionsPressedByKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_LEFT, KMOD_LSHIFT, none),
	          BindableActionMap::MOVE_LEFT);
	EXPECT_EQ(ActionsPressedByKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_RCTRL, KMOD_RCTRL, none), BindableActionMap::ZOOM_ON);
	// A held action and a one-press action on the same key both act
	auto bindings = k_DefaultKeyBindings;
	bindings.at(*IndexOf(bindings, BindableActionMap::QUICK_SAVE)).key = KeyChord {SDL_SCANCODE_LEFT, Modifier::Ctrl};
	EXPECT_EQ(ActionsPressedByKeyDown(bindings, SDL_SCANCODE_LEFT, KMOD_LCTRL, none),
	          static_cast<BindableActionMap>(static_cast<uint64_t>(BindableActionMap::MOVE_LEFT) |
	                                         static_cast<uint64_t>(BindableActionMap::QUICK_SAVE)));
	// A held action with a modifier needs it
	bindings.at(*IndexOf(bindings, BindableActionMap::TILT_UP)).key = KeyChord {SDL_SCANCODE_A, Modifier::Alt};
	EXPECT_EQ(ActionsPressedByKeyDown(bindings, SDL_SCANCODE_A, KMOD_NONE, none), none);
	EXPECT_EQ(ActionsPressedByKeyDown(bindings, SDL_SCANCODE_A, KMOD_LALT | KMOD_LSHIFT, none), BindableActionMap::TILT_UP);
}
