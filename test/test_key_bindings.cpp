/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <SDL_events.h>
#include <gtest/gtest.h>

#include "Input/GameActionMap.h"
#include "Input/KeyBindings.h"

using namespace openblack::input;

namespace
{
SDL_Event KeyDown(SDL_Scancode key, uint16_t modifiers)
{
	SDL_Event event {};
	event.type = SDL_KEYDOWN;
	event.key.keysym.scancode = key;
	event.key.keysym.mod = modifiers;
	return event;
}

SDL_Event MouseEvent(MouseInput mouse)
{
	SDL_Event event {};
	switch (mouse)
	{
	case MouseInput::WheelUp:
	case MouseInput::WheelDown:
		event.type = SDL_MOUSEWHEEL;
		event.wheel.y = mouse == MouseInput::WheelUp ? 1 : -1;
		break;
	default:
		event.type = SDL_MOUSEBUTTONDOWN;
		event.button.clicks = 1;
		event.button.button = mouse == MouseInput::LeftButton     ? SDL_BUTTON_LEFT
		                      : mouse == MouseInput::MiddleButton ? SDL_BUTTON_MIDDLE
		                                                          : SDL_BUTTON_RIGHT;
		break;
	}
	return event;
}

uint16_t SdlModifier(Modifier modifier)
{
	switch (modifier)
	{
	case Modifier::Ctrl:
		return KMOD_LCTRL;
	case Modifier::Shift:
		return KMOD_LSHIFT;
	case Modifier::Alt:
		return KMOD_LALT;
	default:
		return KMOD_NONE;
	}
}
} // namespace

/// One row for each bindable action
TEST(KeyBindings, EveryActionOnce)
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

/// The table the Key Bindings window shows is the game's action map: each row's key, with its modifier, or its mouse
/// input makes our action map hold that row's action
TEST(KeyBindings, TableIsTheGamesActionMap)
{
	for (const auto& binding : k_DefaultKeyBindings)
	{
		GameActionMap actions;
		if (binding.key.has_value())
		{
			actions.ProcessEvent(KeyDown(binding.key->key, SdlModifier(binding.key->modifier)));
		}
		else
		{
			actions.ProcessEvent(MouseEvent(binding.mouse));
		}
		EXPECT_TRUE(actions.GetBindable(binding.action)) << binding.name;
	}
}

/// Where the table names the right Ctrl or Shift, the left one does the same
TEST(KeyBindings, EitherSideOfAModifier)
{
	for (const auto key : {SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL})
	{
		GameActionMap actions;
		actions.ProcessEvent(KeyDown(key, KMOD_NONE));
		EXPECT_TRUE(actions.GetBindable(BindableActionMap::ZOOM_ON));
	}
	for (const auto key : {SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT})
	{
		GameActionMap actions;
		actions.ProcessEvent(KeyDown(key, KMOD_NONE));
		EXPECT_TRUE(actions.GetBindable(BindableActionMap::ROTATE_ON));
	}
}

TEST(KeyBindings, Names)
{
	EXPECT_EQ(KeyChordName({.key = SDL_SCANCODE_S, .modifier = Modifier::Ctrl}), "Ctrl+S");
	EXPECT_EQ(KeyChordName({.key = SDL_SCANCODE_F1}), "F1");
	EXPECT_EQ(KeyChordName({.key = SDL_SCANCODE_RCTRL}), "Ctrl");
	EXPECT_EQ(MouseInputName(MouseInput::WheelUp), "Wheel Up");
	EXPECT_EQ(CategoryName(BindCategory::Places), "Places");
}

TEST(KeyBindings, DefaultsDoNotConflict)
{
	EXPECT_TRUE(FindConflicts(k_DefaultKeyBindings).empty());
	// Ctrl+S quick saves rather than also showing villagers' details
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_S, KMOD_LCTRL), BindableActionMap::QUICK_SAVE);
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_S, KMOD_NONE), BindableActionMap::SHOW_VILLAGER_DETAILS);
}
