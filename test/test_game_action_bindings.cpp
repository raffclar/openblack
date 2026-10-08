/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The action map from the key bindings table: keys, chords and the mouse to actions, rebinding, and the debug window's
// test presses (raffclar's tests of his key bindings)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <SDL_events.h>
#include <gtest/gtest.h>

#include "Input/GameActionMap.h"
#include "Input/KeyBindings.h"

using namespace openblack;
using namespace openblack::input;

namespace
{
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
