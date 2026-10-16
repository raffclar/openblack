/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Temple/TempleEntranceTap.h"

using namespace openblack::temple;

TEST(TempleEntranceTap, TheOwnEntranceTakesThePlayerIntoTheTemple)
{
	const auto tap = TapEntrance({.scriptLetsTap = true, .ownTemple = true, .leashHeld = false});
	EXPECT_TRUE(tap.tapped);
	EXPECT_TRUE(tap.enters);
}

TEST(TempleEntranceTap, AnEntranceOfAnotherPlayerIsTappedButNotEntered)
{
	const auto tap = TapEntrance({.scriptLetsTap = true, .ownTemple = false, .leashHeld = false});
	EXPECT_TRUE(tap.tapped);
	EXPECT_FALSE(tap.enters);
}

TEST(TempleEntranceTap, TheScriptsCanStopTheTap)
{
	const auto tap = TapEntrance({.scriptLetsTap = false, .ownTemple = true, .leashHeld = false});
	EXPECT_FALSE(tap.tapped);
	EXPECT_FALSE(tap.enters);
}

TEST(TempleEntranceTap, TheHeldLeashTakesTheTapAsAnOrder)
{
	const auto tap = TapEntrance({.scriptLetsTap = true, .ownTemple = true, .leashHeld = true});
	EXPECT_FALSE(tap.tapped);
	EXPECT_FALSE(tap.enters);
}
