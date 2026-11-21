/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS 1

#include <cstdint>

#include <array>

#include <SDL_events.h>
#include <SDL_mouse.h>
#include <gtest/gtest.h>

#include "Input/GameActionMap.h"
#include "Input/InjectedInput.h"
#include "Input/MouseClicks.h"

using namespace openblack::input;
using mouse_clicks::Clicks;
using mouse_clicks::k_DoublePress;
using mouse_clicks::k_PlainPress;

namespace
{

[[nodiscard]] mouse_clicks::Press LeftAt(uint32_t milliseconds, glm::ivec2 position = {100, 100})
{
	return {.button = SDL_BUTTON_LEFT, .milliseconds = milliseconds, .position = position};
}

[[nodiscard]] SDL_Event ButtonEvent(bool down, uint8_t button, uint32_t milliseconds, glm::ivec2 position, uint8_t clicks)
{
	SDL_Event event {};
	event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
	event.button.timestamp = milliseconds;
	event.button.button = button;
	event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.button.clicks = clicks;
	event.button.x = position.x;
	event.button.y = position.y;
	return event;
}

} // namespace

TEST(MouseClicks, TheWindowsDefaultsAreHalfASecondAndAFourPixelBox)
{
	EXPECT_EQ(mouse_clicks::k_WindowsDefaults.milliseconds, 500u);
	EXPECT_EQ(mouse_clicks::k_WindowsDefaults.box, glm::ivec2(4, 4));
}

TEST(MouseClicks, AfterADoubleClickTheNextPressStartsANewPair)
{
	Clicks clicks;
	EXPECT_EQ(clicks.Down(LeftAt(1000)), k_PlainPress);
	EXPECT_EQ(clicks.Down(LeftAt(1100)), k_DoublePress);
	// The third quick press is a plain press, and the fourth is a double click again
	EXPECT_EQ(clicks.Down(LeftAt(1200)), k_PlainPress);
	EXPECT_EQ(clicks.Down(LeftAt(1300)), k_DoublePress);
	EXPECT_EQ(clicks.Down(LeftAt(1400)), k_PlainPress);
}

TEST(MouseClicks, TheSecondPressMustComeSoonerThanTheTime)
{
	Clicks slow;
	EXPECT_EQ(slow.Down(LeftAt(1000)), k_PlainPress);
	EXPECT_EQ(slow.Down(LeftAt(1500)), k_PlainPress);
	// The slow press is the first press of a new pair
	EXPECT_EQ(slow.Down(LeftAt(1999)), k_DoublePress);
}

TEST(MouseClicks, TheSecondPressMustBeInTheBoxOnTheFirstPress)
{
	// The box is four pixels wide: the second press can be one pixel from the first, not two
	Clicks near;
	EXPECT_EQ(near.Down(LeftAt(0, {100, 100})), k_PlainPress);
	EXPECT_EQ(near.Down(LeftAt(10, {101, 99})), k_DoublePress);
	Clicks across;
	EXPECT_EQ(across.Down(LeftAt(0, {100, 100})), k_PlainPress);
	EXPECT_EQ(across.Down(LeftAt(10, {102, 100})), k_PlainPress);
	Clicks down;
	EXPECT_EQ(down.Down(LeftAt(0, {100, 100})), k_PlainPress);
	EXPECT_EQ(down.Down(LeftAt(10, {100, 98})), k_PlainPress);
	// A larger box of the system takes presses farther away
	Clicks large({.milliseconds = 500, .box = {32, 32}});
	EXPECT_EQ(large.Down(LeftAt(0, {100, 100})), k_PlainPress);
	EXPECT_EQ(large.Down(LeftAt(10, {115, 85})), k_DoublePress);
}

TEST(MouseClicks, APressOfAnotherButtonEndsThePair)
{
	Clicks clicks;
	EXPECT_EQ(clicks.Down(LeftAt(0)), k_PlainPress);
	EXPECT_EQ(clicks.Down({.button = SDL_BUTTON_RIGHT, .milliseconds = 50, .position = {100, 100}}), k_PlainPress);
	EXPECT_EQ(clicks.Down(LeftAt(100)), k_PlainPress);
	EXPECT_EQ(clicks.Down(LeftAt(150)), k_DoublePress);
	// The right button makes its own double clicks
	EXPECT_EQ(clicks.Down({.button = SDL_BUTTON_RIGHT, .milliseconds = 200, .position = {100, 100}}), k_PlainPress);
	EXPECT_EQ(clicks.Down({.button = SDL_BUTTON_RIGHT, .milliseconds = 250, .position = {100, 100}}), k_DoublePress);
}

TEST(MouseClicks, TheTimeContinuesPastTheEndOfTheClock)
{
	Clicks clicks;
	EXPECT_EQ(clicks.Down(LeftAt(0xFFFFFF00u)), k_PlainPress);
	EXPECT_EQ(clicks.Down(LeftAt(0x00000010u)), k_DoublePress);
}

TEST(MouseClicks, OnlyAPlainPressHoldsItsButton)
{
	constexpr auto k_Left = static_cast<uint32_t>(SDL_BUTTON(SDL_BUTTON_LEFT));
	constexpr auto k_Right = static_cast<uint32_t>(SDL_BUTTON(SDL_BUTTON_RIGHT));
	Clicks clicks;
	EXPECT_EQ(clicks.Down(LeftAt(0)), k_PlainPress);
	EXPECT_EQ(clicks.HeldButtons(), k_Left);
	EXPECT_EQ(clicks.Up(SDL_BUTTON_LEFT), k_PlainPress);
	EXPECT_EQ(clicks.HeldButtons(), 0u);
	// The second press of the double click does not hold the button, and its letting go tells the double click
	EXPECT_EQ(clicks.Down(LeftAt(100)), k_DoublePress);
	EXPECT_EQ(clicks.HeldButtons(), 0u);
	EXPECT_EQ(clicks.Up(SDL_BUTTON_LEFT), k_DoublePress);
	// The third press holds the button again
	EXPECT_EQ(clicks.Down(LeftAt(200)), k_PlainPress);
	EXPECT_EQ(clicks.HeldButtons(), k_Left);
	EXPECT_EQ(clicks.Down({.button = SDL_BUTTON_RIGHT, .milliseconds = 300}), k_PlainPress);
	EXPECT_EQ(clicks.HeldButtons(), k_Left | k_Right);
	EXPECT_EQ(clicks.Up(SDL_BUTTON_RIGHT), k_PlainPress);
	EXPECT_EQ(clicks.HeldButtons(), k_Left);
}

TEST(MouseClicks, AToolCountsAsTheWindowSystemDoes)
{
	EXPECT_EQ(mouse_clicks::ClicksOfToolPress(1), k_PlainPress);
	EXPECT_EQ(mouse_clicks::ClicksOfToolPress(2), k_DoublePress);
	EXPECT_EQ(mouse_clicks::ClicksOfToolPress(3), k_PlainPress);
	EXPECT_EQ(mouse_clicks::ClicksOfToolPress(4), k_DoublePress);
	EXPECT_EQ(mouse_clicks::ClicksOfToolPress(0), k_PlainPress);
	// A press of a tool ends the pair that waits
	Clicks clicks;
	EXPECT_EQ(clicks.Down(LeftAt(0)), k_PlainPress);
	EXPECT_EQ(clicks.ToolDown(SDL_BUTTON_LEFT, 1), k_PlainPress);
	EXPECT_EQ(clicks.Down(LeftAt(20)), k_PlainPress);
	EXPECT_TRUE(mouse_clicks::IsDoublePress(clicks.ToolDown(SDL_BUTTON_LEFT, 2)));
	EXPECT_EQ(clicks.HeldButtons(), static_cast<uint32_t>(SDL_BUTTON(SDL_BUTTON_LEFT)));
}

TEST(MouseClicks, TheActionMapGivesTheWindowsCountToEachEvent)
{
	GameActionMap map;
	// The window system counts 1, 2, 3, 4. The game gets plain, double, plain, double.
	constexpr std::array<uint8_t, 4> k_WindowCounts {1, 2, 3, 4};
	constexpr std::array<uint8_t, 4> k_Expected {k_PlainPress, k_DoublePress, k_PlainPress, k_DoublePress};
	for (size_t i = 0; i < k_WindowCounts.size(); ++i)
	{
		const auto at = static_cast<uint32_t>(1000 + 100 * i);
		auto down = ButtonEvent(true, SDL_BUTTON_LEFT, at, {50, 60}, k_WindowCounts.at(i));
		map.PairClicks(down);
		EXPECT_EQ(down.button.clicks, k_Expected.at(i)) << i;
		auto up = ButtonEvent(false, SDL_BUTTON_LEFT, at + 30, {50, 60}, k_WindowCounts.at(i));
		map.PairClicks(up);
		EXPECT_EQ(up.button.clicks, k_Expected.at(i)) << i;
	}
	// The window system's box is larger: the game takes a press two pixels away as a plain press
	auto far = ButtonEvent(true, SDL_BUTTON_LEFT, 2000, {50, 60}, 1);
	map.PairClicks(far);
	auto second = ButtonEvent(true, SDL_BUTTON_LEFT, 2100, {52, 60}, 2);
	map.PairClicks(second);
	EXPECT_EQ(second.button.clicks, k_PlainPress);
	// A press that a tool made keeps the count that the tool wants
	auto tool = ButtonEvent(true, SDL_BUTTON_LEFT, 9000, {0, 0}, 2);
	MarkInjected(tool);
	map.PairClicks(tool);
	EXPECT_EQ(tool.button.clicks, k_DoublePress);
	EXPECT_EQ(map.PlainHeldButtons(), static_cast<uint32_t>(SDL_BUTTON(SDL_BUTTON_LEFT)));
	// Other events do not change
	SDL_Event motion {};
	motion.type = SDL_MOUSEMOTION;
	map.PairClicks(motion);
	EXPECT_EQ(motion.type, SDL_MOUSEMOTION);
}
