/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <tuple>

#include <SDL_events.h>
#include <gtest/gtest.h>

#include "Common/Platform.h"
#include "Input/AltHeldPresses.h"
#include "Input/GameActionMap.h"
#include "Input/KeyBindings.h"

using namespace openblack::input;
using alt_presses::AltHeldPresses;
using alt_presses::Source;
using alt_presses::SourceActions;
using openblack::Platform;

namespace
{

// Each mouse input has its own one-press action bit
constexpr SourceActions k_Actions {0x1, 0x2, 0x4, 0x8, 0x10};

[[nodiscard]] SDL_Event ButtonEvent(bool down, uint8_t button)
{
	SDL_Event event {};
	event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
	event.button.button = button;
	event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.button.clicks = 1;
	return event;
}

[[nodiscard]] SDL_Event AltEvent(bool down)
{
	SDL_Event event {};
	event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	event.key.keysym.scancode = SDL_SCANCODE_LALT;
	event.key.keysym.mod = down ? KMOD_LALT : KMOD_NONE;
	return event;
}

[[nodiscard]] SDL_Event WheelEvent(int y)
{
	SDL_Event event {};
	event.type = SDL_MOUSEWHEEL;
	event.wheel.y = y;
	event.wheel.preciseY = static_cast<float>(y);
	event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
	return event;
}

/// The game's defaults, with Help on the middle button and on a turn of the wheel up
[[nodiscard]] KeyBindingTable HelpOnTheMouse(MouseInput mouse)
{
	auto table = DefaultKeyBindings(Platform::Desktop, true);
	for (auto& binding : table)
	{
		if (binding.mouse == mouse)
		{
			binding.mouse = MouseInput::None;
		}
	}
	std::ranges::find(table, BindableActionMap::HELP, &KeyBinding::action)->mouse = mouse;
	return table;
}

/// The frame starts, the frame's events come, and then the game reads the actions
[[nodiscard]] bool HelpPressed(const GameActionMap& map)
{
	return map.Get(BindableActionMap::HELP) && map.GetChanged(BindableActionMap::HELP);
}

} // namespace

TEST(AltHeldPresses, WithoutAltAPressActsAtOnce)
{
	AltHeldPresses presses;
	EXPECT_EQ(presses.Press(Source::LeftButton, false, k_Actions), 0x1u);
	EXPECT_EQ(presses.Press(Source::WheelUp, false, k_Actions), 0x8u);
	EXPECT_FALSE(presses.HasKeptPresses());
	EXPECT_EQ(presses.NewFrame(false, k_Actions), 0u);
}

TEST(AltHeldPresses, WithAltAButtonPressWaitsUntilAltGoesUp)
{
	AltHeldPresses presses;
	EXPECT_EQ(presses.Press(Source::RightButton, true, k_Actions), 0u);
	EXPECT_TRUE(presses.HasKeptPresses());
	// The press waits for as many frames as Alt stays down
	EXPECT_EQ(presses.NewFrame(true, k_Actions), 0u);
	EXPECT_EQ(presses.NewFrame(true, k_Actions), 0u);
	// The first frame without Alt does the action, once
	EXPECT_EQ(presses.NewFrame(false, k_Actions), 0x2u);
	EXPECT_EQ(presses.NewFrame(false, k_Actions), 0u);
}

TEST(AltHeldPresses, TheGameKeepsOnePressForEachButton)
{
	AltHeldPresses presses;
	std::ignore = presses.Press(Source::LeftButton, true, k_Actions);
	std::ignore = presses.NewFrame(true, k_Actions);
	std::ignore = presses.Press(Source::LeftButton, true, k_Actions);
	EXPECT_EQ(presses.NewFrame(false, k_Actions), 0x1u);
	EXPECT_EQ(presses.NewFrame(false, k_Actions), 0u);
}

TEST(AltHeldPresses, KeptPressesActOneEachFrameLeftThenRightThenMiddle)
{
	AltHeldPresses presses;
	std::ignore = presses.Press(Source::MiddleButton, true, k_Actions);
	std::ignore = presses.Press(Source::RightButton, true, k_Actions);
	std::ignore = presses.Press(Source::LeftButton, true, k_Actions);
	EXPECT_EQ(presses.NewFrame(false, k_Actions), 0x1u);
	EXPECT_EQ(presses.NewFrame(false, k_Actions), 0x2u);
	EXPECT_EQ(presses.NewFrame(false, k_Actions), 0x4u);
	EXPECT_FALSE(presses.HasKeptPresses());
}

TEST(AltHeldPresses, AKeptPressOfAButtonWithNoOnePressActionGoesWithoutAFrame)
{
	constexpr SourceActions k_LeftHasNone {0x0, 0x2, 0x4, 0x8, 0x10};
	AltHeldPresses presses;
	std::ignore = presses.Press(Source::LeftButton, true, k_LeftHasNone);
	std::ignore = presses.Press(Source::RightButton, true, k_LeftHasNone);
	EXPECT_EQ(presses.NewFrame(false, k_LeftHasNone), 0x2u);
	EXPECT_FALSE(presses.HasKeptPresses());
}

TEST(AltHeldPresses, WithAltATurnOfTheWheelLosesItsOnePressAction)
{
	AltHeldPresses presses;
	EXPECT_EQ(presses.Press(Source::WheelDown, true, k_Actions), 0u);
	EXPECT_FALSE(presses.HasKeptPresses());
	EXPECT_EQ(presses.NewFrame(false, k_Actions), 0u);
}

TEST(AltHeldPresses, AltDownInTheFrameOfAPressStopsThePress)
{
	AltHeldPresses presses;
	EXPECT_EQ(presses.Press(Source::LeftButton, false, k_Actions), 0x1u);
	EXPECT_EQ(presses.Press(Source::WheelUp, false, k_Actions), 0x8u);
	// The game reads Alt at the end of the frame, so both presses did not act: the button waits, the wheel is lost
	EXPECT_EQ(presses.AltDown(k_Actions), 0x9u);
	EXPECT_TRUE(presses.HasKeptPresses());
	EXPECT_EQ(presses.NewFrame(true, k_Actions), 0u);
	EXPECT_EQ(presses.NewFrame(false, k_Actions), 0x1u);
}

TEST(AltHeldPresses, AltUpInTheFrameOfAPressLetsThePressAct)
{
	AltHeldPresses presses;
	std::ignore = presses.Press(Source::WheelUp, true, k_Actions);
	std::ignore = presses.Press(Source::LeftButton, true, k_Actions);
	EXPECT_EQ(presses.AltUp(k_Actions), 0x9u);
	EXPECT_FALSE(presses.HasKeptPresses());
	EXPECT_EQ(presses.NewFrame(false, k_Actions), 0u);
}

TEST(AltHeldPresses, OnePressActionsLeaveOutTheHeldActions)
{
	const auto actions = static_cast<BindableActionMap>(static_cast<uint64_t>(BindableActionMap::HELP) |
	                                                    static_cast<uint64_t>(BindableActionMap::ZOOM_IN) |
	                                                    static_cast<uint64_t>(BindableActionMap::NEXT_LEASH));
	EXPECT_EQ(OnePressActions(actions), static_cast<BindableActionMap>(static_cast<uint64_t>(BindableActionMap::HELP) |
	                                                                   static_cast<uint64_t>(BindableActionMap::NEXT_LEASH)));
	EXPECT_EQ(OnePressActions(BindableActionMap::ROTATE_AROUND_MOUSE_ON), BindableActionMap::NONE);
}

TEST(GameActionMap, WithAltHeldAMouseButtonsOnePressActionWaitsForAlt)
{
	GameActionMap map;
	map.SetKeyBindings(HelpOnTheMouse(MouseInput::MiddleButton));
	map.Frame();
	map.ProcessEvent(AltEvent(true));
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_MIDDLE));
	EXPECT_FALSE(HelpPressed(map));
	map.Frame();
	map.ProcessEvent(ButtonEvent(false, SDL_BUTTON_MIDDLE));
	EXPECT_FALSE(HelpPressed(map));
	map.Frame();
	// Alt goes up: the press acts in this frame, from the mouse, and the next frame lets go of it
	map.ProcessEvent(AltEvent(false));
	EXPECT_TRUE(HelpPressed(map));
	EXPECT_TRUE(map.WaitedMousePress(BindableActionMap::HELP));
	map.Frame();
	EXPECT_FALSE(map.Get(BindableActionMap::HELP));
	EXPECT_FALSE(map.WaitedMousePress(BindableActionMap::HELP));
}

TEST(GameActionMap, WithoutAltAMouseButtonsOnePressActionActsAtOnce)
{
	GameActionMap map;
	map.SetKeyBindings(HelpOnTheMouse(MouseInput::MiddleButton));
	map.Frame();
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_MIDDLE));
	EXPECT_TRUE(HelpPressed(map));
	EXPECT_FALSE(map.WaitedMousePress(BindableActionMap::HELP));
}

TEST(GameActionMap, WithAltHeldTheHeldActionsOfTheMouseStillAct)
{
	GameActionMap map;
	map.Frame();
	map.ProcessEvent(AltEvent(true));
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_MIDDLE));
	EXPECT_TRUE(map.Get(BindableActionMap::ROTATE_AROUND_MOUSE_ON));
	// Release the middle button. A left press while the middle button is held is a press of the two buttons together.
	map.ProcessEvent(ButtonEvent(false, SDL_BUTTON_MIDDLE));
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT));
	EXPECT_TRUE(map.Get(BindableActionMap::MOVE));
	map.ProcessEvent(WheelEvent(1));
	EXPECT_TRUE(map.Get(BindableActionMap::ZOOM_IN));
}

TEST(GameActionMap, WithAltHeldATurnOfTheWheelLosesItsOnePressAction)
{
	GameActionMap map;
	map.SetKeyBindings(HelpOnTheMouse(MouseInput::WheelUp));
	map.Frame();
	map.ProcessEvent(AltEvent(true));
	map.ProcessEvent(WheelEvent(1));
	EXPECT_FALSE(HelpPressed(map));
	map.Frame();
	map.ProcessEvent(AltEvent(false));
	map.Frame();
	EXPECT_FALSE(map.Get(BindableActionMap::HELP));
}
