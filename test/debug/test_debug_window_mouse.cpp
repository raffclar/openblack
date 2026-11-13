/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <Debug/DebugWindowMouse.h>
#include <Input/InjectedInput.h>
#include <Input/ScriptedMouse.h>
#include <SDL_events.h>
#include <gtest/gtest.h>

using namespace openblack;
using debug::gui::ChooseWindowMouse;
using debug::gui::WindowMouse;
using debug::gui::WindowsTakeEvent;

namespace
{

constexpr std::array k_MouseTypes {SDL_MOUSEMOTION, SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP, SDL_MOUSEWHEEL};

SDL_Event MouseEvent(SDL_EventType type, bool injected)
{
	SDL_Event event {};
	event.type = type;
	if (injected)
	{
		input::MarkInjected(event);
	}
	return event;
}

SDL_Event WindowEvent(SDL_WindowEventID id)
{
	SDL_Event event {};
	event.type = SDL_WINDOWEVENT;
	event.window.event = static_cast<uint8_t>(id);
	return event;
}

} // namespace

// A scenario that drives the hand has priority. The inspector and the lock make the windows follow the driven pointer.
TEST(DebugWindowMouse, AScenarioHasPriorityOverTheInspectorAndTheLock)
{
	const auto choose = [](bool scenario, bool driven, bool locked) {
		return ChooseWindowMouse(
		    {.scenarioDrivesHand = scenario, .handDemoPlays = false, .pointerDriven = driven, .inputLocked = locked});
	};
	EXPECT_EQ(choose(false, false, false), WindowMouse::Player);
	EXPECT_EQ(choose(false, true, false), WindowMouse::DrivenOnly);
	EXPECT_EQ(choose(false, false, true), WindowMouse::DrivenOnly);
	EXPECT_EQ(choose(false, true, true), WindowMouse::DrivenOnly);
	EXPECT_EQ(choose(true, true, true), WindowMouse::KeptOff);
	EXPECT_EQ(choose(true, false, false), WindowMouse::KeptOff);
}

// While a hand demonstration plays, the windows take no press. A debug window under the recorded pointer then does not
// stop the cast of the demonstration.
TEST(DebugWindowMouse, AHandDemonstrationKeepsTheWindowsOffTheMouse)
{
	for (const bool driven : {false, true})
	{
		for (const bool locked : {false, true})
		{
			EXPECT_EQ(ChooseWindowMouse(
			              {.scenarioDrivesHand = false, .handDemoPlays = true, .pointerDriven = driven, .inputLocked = locked}),
			          WindowMouse::KeptOff);
		}
	}
	EXPECT_FALSE(WindowsTakeEvent(MouseEvent(SDL_MOUSEBUTTONDOWN, true), WindowMouse::KeptOff));
	EXPECT_TRUE(WindowsTakeEvent(MouseEvent(SDL_MOUSEBUTTONUP, true), WindowMouse::KeptOff));
}

// With the mouse of the player, the windows take all the mouse events
TEST(DebugWindowMouse, ThePlayersMouseGoesToTheWindows)
{
	for (const auto type : k_MouseTypes)
	{
		EXPECT_TRUE(WindowsTakeEvent(MouseEvent(type, false), WindowMouse::Player));
		EXPECT_TRUE(WindowsTakeEvent(MouseEvent(type, true), WindowMouse::Player));
	}
	EXPECT_TRUE(WindowsTakeEvent(WindowEvent(SDL_WINDOWEVENT_LEAVE), WindowMouse::Player));
}

// While the inspector drives the pointer, its moves, presses and wheel turns go to the windows. The real mouse does not
// move the pointer of the windows, and does not take it out of the window.
TEST(DebugWindowMouse, TheDrivenPointerAloneGoesToTheWindows)
{
	for (const auto type : k_MouseTypes)
	{
		EXPECT_TRUE(WindowsTakeEvent(MouseEvent(type, true), WindowMouse::DrivenOnly)) << type;
		EXPECT_FALSE(WindowsTakeEvent(MouseEvent(type, false), WindowMouse::DrivenOnly)) << type;
	}
	auto pumped = input::PumpedButton(SDL_BUTTON_LEFT, true, 1, 0);
	EXPECT_TRUE(WindowsTakeEvent(pumped, WindowMouse::DrivenOnly));
	auto motion = input::PumpedMotion({4, 2}, 0);
	EXPECT_TRUE(WindowsTakeEvent(motion, WindowMouse::DrivenOnly));
	EXPECT_FALSE(WindowsTakeEvent(WindowEvent(SDL_WINDOWEVENT_ENTER), WindowMouse::DrivenOnly));
	EXPECT_FALSE(WindowsTakeEvent(WindowEvent(SDL_WINDOWEVENT_LEAVE), WindowMouse::DrivenOnly));
}

// Kept off the mouse, the windows take no press and no wheel turn. Moves and releases still go to them, so that no
// button of the windows stays down.
TEST(DebugWindowMouse, KeptOffTheWindowsTakeNoPress)
{
	for (const bool injected : {false, true})
	{
		EXPECT_FALSE(WindowsTakeEvent(MouseEvent(SDL_MOUSEBUTTONDOWN, injected), WindowMouse::KeptOff));
		EXPECT_FALSE(WindowsTakeEvent(MouseEvent(SDL_MOUSEWHEEL, injected), WindowMouse::KeptOff));
		EXPECT_TRUE(WindowsTakeEvent(MouseEvent(SDL_MOUSEBUTTONUP, injected), WindowMouse::KeptOff));
		EXPECT_TRUE(WindowsTakeEvent(MouseEvent(SDL_MOUSEMOTION, injected), WindowMouse::KeptOff));
	}
}

// Typed text, keys and the focus of the window go to the windows in each mode
TEST(DebugWindowMouse, OtherEventsAlwaysGoToTheWindows)
{
	for (const auto mouse : {WindowMouse::Player, WindowMouse::DrivenOnly, WindowMouse::KeptOff})
	{
		for (const auto type : {SDL_TEXTINPUT, SDL_KEYDOWN, SDL_KEYUP})
		{
			SDL_Event event {};
			event.type = type;
			EXPECT_TRUE(WindowsTakeEvent(event, mouse));
		}
		EXPECT_TRUE(WindowsTakeEvent(WindowEvent(SDL_WINDOWEVENT_FOCUS_LOST), mouse));
	}
}
