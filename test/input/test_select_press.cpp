/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Input/SelectPress.h"

using openblack::input::select_press::Event;
using openblack::input::select_press::Facts;
using openblack::input::select_press::Next;

TEST(SelectPress, APressThatGripsTheLandAloneSetsTheFlag)
{
	EXPECT_TRUE(Next(false, {.event = Event::SelectPress, .gripsLandAlone = true, .selectDown = true}));
	// A press that does not grip the land, or grips it with the other button down, does not keep the flag
	EXPECT_FALSE(Next(true, {.event = Event::SelectPress, .gripsLandAlone = false, .selectDown = true}));
}

TEST(SelectPress, APressWhileTheInterfaceIsOffDoesNotSetTheFlag)
{
	EXPECT_FALSE(Next(false, {.event = Event::SelectPress, .interfaceOn = false, .gripsLandAlone = true, .selectDown = true}));
}

TEST(SelectPress, TheLetGoAndThePressOfTheOtherButtonClearTheFlag)
{
	EXPECT_FALSE(Next(true, {.event = Event::SelectLetGo, .gripsLandAlone = false, .selectDown = false}));
	EXPECT_FALSE(Next(true, {.event = Event::OtherPress, .gripsLandAlone = false, .selectDown = true}));
}

TEST(SelectPress, TheFlagStaysWhileTheSelectButtonStaysDown)
{
	EXPECT_TRUE(Next(true, {.event = Event::Other, .gripsLandAlone = true, .selectDown = true}));
	// The second press of a double click changes nothing
	EXPECT_TRUE(Next(true, {.event = Event::SelectDoublePress, .gripsLandAlone = true, .selectDown = true}));
	EXPECT_FALSE(Next(false, {.event = Event::SelectDoublePress, .gripsLandAlone = true, .selectDown = true}));
	// A let go that another window took clears the flag
	EXPECT_FALSE(Next(true, {.event = Event::Other, .gripsLandAlone = false, .selectDown = false}));
}
