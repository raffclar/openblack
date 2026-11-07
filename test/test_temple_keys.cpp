/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Temple/TempleKeys.h"

using openblack::temple::keys::EscapeAsksNewGame;
using openblack::temple::keys::EscapeFacts;
using openblack::temple::keys::EscapeLeavesRoom;
using openblack::temple::keys::RoomKeyActs;
using openblack::temple::keys::RoomKeyFacts;

TEST(TempleKeys, InsideTheTempleARoomKeyGoesToItsRoom)
{
	EXPECT_TRUE(RoomKeyActs({.insideTemple = true}));
}

TEST(TempleKeys, InsideTheTempleTheRoomKeysDoNothingOnceTheGameIsOver)
{
	EXPECT_FALSE(RoomKeyActs({.insideTemple = true, .gameOver = true}));
}

TEST(TempleKeys, TheRoomKeysDoNothingWhileAScriptHasTheCinemaBarsIn)
{
	EXPECT_FALSE(RoomKeyActs({.insideTemple = true, .scriptBars = true}));
	EXPECT_FALSE(RoomKeyActs({.scriptBars = true, .templeBuilt = true}));
	EXPECT_FALSE(RoomKeyActs({.scriptBars = true, .gameOver = true}));
}

TEST(TempleKeys, OutsideARoomKeyNeedsATempleThatStandsWhollyBuilt)
{
	EXPECT_FALSE(RoomKeyActs({}));
	EXPECT_TRUE(RoomKeyActs({.templeBuilt = true}));
}

TEST(TempleKeys, OutsideARoomKeyTakesThePlayerInOnceTheGameIsOver)
{
	EXPECT_TRUE(RoomKeyActs({.gameOver = true}));
}

TEST(TempleKeys, EscapeWithNoModifierLeavesTheRoom)
{
	EXPECT_TRUE(EscapeLeavesRoom({}));
}

TEST(TempleKeys, EscapeWithShiftCtrlOrAltDoesNotLeaveTheRoom)
{
	EXPECT_FALSE(EscapeLeavesRoom({.modifierHeld = true}));
}

TEST(TempleKeys, EscapeDoesNothingWhileAScriptHasTheCinemaBarsIn)
{
	EXPECT_FALSE(EscapeLeavesRoom({.scriptBars = true}));
}

TEST(TempleKeys, EscapeDoesNotLeaveTheRoomOnceTheGameIsOver)
{
	EXPECT_FALSE(EscapeLeavesRoom({.gameOver = true}));
}

TEST(TempleKeys, EscapeAsksAboutANewGameOnceTheGameIsOver)
{
	EXPECT_TRUE(EscapeAsksNewGame({.gameOver = true}));
	EXPECT_TRUE(EscapeAsksNewGame({.modifierHeld = true, .gameOver = true}));
}

TEST(TempleKeys, EscapeAsksNothingBeforeTheGameIsOverOrUnderTheCinemaBars)
{
	EXPECT_FALSE(EscapeAsksNewGame({}));
	EXPECT_FALSE(EscapeAsksNewGame({.scriptBars = true, .gameOver = true}));
}
