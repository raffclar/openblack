/*******************************************************************************
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
#include <SDL_mouse.h>
#include <gtest/gtest.h>

#include "Input/GameActionMap.h"

using namespace openblack::input;

namespace
{

[[nodiscard]] SDL_Event ButtonEvent(bool down, uint8_t button, uint8_t clicks)
{
	SDL_Event event {};
	event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
	event.button.button = button;
	event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.button.clicks = clicks;
	return event;
}

/// A click, then the second press of the double click, each let go of a frame later
void DoubleClick(GameActionMap& map, uint8_t button)
{
	map.ProcessEvent(ButtonEvent(true, button, 1));
	map.Frame();
	map.ProcessEvent(ButtonEvent(false, button, 1));
	map.Frame();
	map.ProcessEvent(ButtonEvent(true, button, 2));
	map.Frame();
	map.ProcessEvent(ButtonEvent(false, button, 2));
	map.Frame();
}

} // namespace

TEST(DoubleClick, SecondPressMarksItUntilTaken)
{
	GameActionMap map;
	map.Frame();
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 1));
	map.ProcessEvent(ButtonEvent(false, SDL_BUTTON_LEFT, 1));
	EXPECT_FALSE(map.Get(UnbindableActionMap::DOUBLE_CLICK));
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 2));
	EXPECT_TRUE(map.Get(UnbindableActionMap::DOUBLE_CLICK));
	EXPECT_TRUE(map.TakeDoubleClick());
	EXPECT_FALSE(map.Get(UnbindableActionMap::DOUBLE_CLICK));
	EXPECT_FALSE(map.TakeDoubleClick());
}

TEST(DoubleClick, LettingGoLeavesItWaiting)
{
	GameActionMap map;
	map.Frame();
	DoubleClick(map, SDL_BUTTON_LEFT);
	// However many frames pass, and whatever the button does, it waits for the camera
	for (int frame = 0; frame < 100; ++frame)
	{
		map.Frame();
	}
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 1));
	map.ProcessEvent(ButtonEvent(false, SDL_BUTTON_LEFT, 1));
	EXPECT_TRUE(map.Get(UnbindableActionMap::DOUBLE_CLICK));
	EXPECT_TRUE(map.TakeDoubleClick());
	EXPECT_FALSE(map.TakeDoubleClick());
}

TEST(DoubleClick, LostLettingGoChangesNothing)
{
	// The second press's letting go may never come, as when it went to a window over the game: the double click is
	// taken all the same, and nothing stays held
	GameActionMap map;
	map.Frame();
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 1));
	map.ProcessEvent(ButtonEvent(false, SDL_BUTTON_LEFT, 1));
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 2));
	map.Frame();
	EXPECT_TRUE(map.TakeDoubleClick());
	map.Frame();
	EXPECT_FALSE(map.Get(UnbindableActionMap::DOUBLE_CLICK));
	EXPECT_FALSE(map.Get(BindableActionMap::MOVE));
}

TEST(DoubleClick, SecondPressPressesNoActions)
{
	GameActionMap map;
	map.Frame();
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 1));
	EXPECT_TRUE(map.Get(BindableActionMap::MOVE));
	map.ProcessEvent(ButtonEvent(false, SDL_BUTTON_LEFT, 1));
	EXPECT_FALSE(map.Get(BindableActionMap::MOVE));
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 2));
	EXPECT_FALSE(map.Get(BindableActionMap::MOVE));
}

TEST(DoubleClick, OnlyTheLeftButtonDoubleClicks)
{
	GameActionMap map;
	map.Frame();
	DoubleClick(map, SDL_BUTTON_RIGHT);
	DoubleClick(map, SDL_BUTTON_MIDDLE);
	EXPECT_FALSE(map.Get(UnbindableActionMap::DOUBLE_CLICK));
	EXPECT_FALSE(map.TakeDoubleClick());
}
