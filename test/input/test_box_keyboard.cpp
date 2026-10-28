/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Input/BoxKeyboard.h"

using namespace openblack::input::box_keyboard;

TEST(BoxKeyboard, NoBoxLeavesTheKeysToTheGame)
{
	EXPECT_FALSE(WantsKeys({}));
}

TEST(BoxKeyboard, EachFrontEndBoxTakesTheKeyboard)
{
	EXPECT_TRUE(WantsKeys({.menu = true}));
	EXPECT_TRUE(WantsKeys({.skipBox = true}));
	EXPECT_TRUE(WantsKeys({.newPlayerBox = true}));
	EXPECT_TRUE(WantsKeys({.skirmishBox = true}));
	EXPECT_TRUE(WantsKeys({.landInfoBox = true}));
	EXPECT_TRUE(WantsKeys({.saveGameBoxes = true}));
	EXPECT_TRUE(WantsKeys({.endGameBox = true}));
	EXPECT_TRUE(WantsKeys({.multiplayerBoxes = true}));
	EXPECT_TRUE(WantsKeys({.tattooEditor = true}));
	EXPECT_TRUE(WantsKeys({.chatTyping = true}));
}

TEST(BoxKeyboard, TheStoryHistoryAndTheMusicControlsLetTheKeysThrough)
{
	EXPECT_FALSE(WantsKeys({.storyHistory = true}));
	EXPECT_FALSE(WantsKeys({.musicControls = true}));
	EXPECT_FALSE(WantsKeys({.storyHistory = true, .musicControls = true}));
	EXPECT_TRUE(WantsKeys({.menu = true, .storyHistory = true}));
}

TEST(BoxKeyboard, TheLatchChangesOnlyWhenAKeyGoesDown)
{
	Latch latch;
	EXPECT_FALSE(latch.Taken());
	// A box opens, but no key went down yet
	latch.KeyDown(false);
	EXPECT_FALSE(latch.Taken());
	// A key goes down while the box shows
	latch.KeyDown(true);
	EXPECT_TRUE(latch.Taken());
	// The box closes with a click: the latch stays until the next key goes down
	EXPECT_TRUE(latch.Taken());
	latch.KeyDown(false);
	EXPECT_FALSE(latch.Taken());
}

TEST(BoxKeyboard, AnAnsweredMessageBoxGivesTheKeyboardBack)
{
	Latch latch;
	latch.KeyDown(true);
	latch.Answered();
	EXPECT_FALSE(latch.Taken());
}
