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

#include <array>
#include <chrono>
#include <numbers>
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
	EXPECT_FALSE(map.Get(BindableActionMap::QUICK_SAVE));
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
