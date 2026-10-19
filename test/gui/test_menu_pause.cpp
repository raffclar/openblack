/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The pause that the game's menu holds while it is open

#include <optional>

#include <Gui/MenuPause.h>
#include <gtest/gtest.h>

using openblack::gui::MenuPause;

TEST(MenuPause, TheMenuPausesTheGameWhenItOpens)
{
	MenuPause pause;
	EXPECT_EQ(pause.Follow(false), std::nullopt);
	EXPECT_EQ(pause.Follow(true), true);
	EXPECT_TRUE(pause.IsHeld());
	// The pause stays while the menu stays open
	EXPECT_EQ(pause.Follow(true), std::nullopt);
}

TEST(MenuPause, TheGameRunsWhenTheMenuCloses)
{
	MenuPause pause;
	(void)pause.Follow(true);
	// The game runs when the menu closes, also when the game was paused before the menu opened
	EXPECT_EQ(pause.Follow(false), false);
	EXPECT_FALSE(pause.IsHeld());
}

TEST(MenuPause, ALandChangeReleasesThePauseOnce)
{
	MenuPause pause;
	(void)pause.Follow(true);
	EXPECT_EQ(pause.Release(), false);
	EXPECT_FALSE(pause.IsHeld());
	// The next frame finds the menu closed and does not change the pause of the new land
	EXPECT_EQ(pause.Follow(false), std::nullopt);
	EXPECT_EQ(pause.Release(), std::nullopt);
}
