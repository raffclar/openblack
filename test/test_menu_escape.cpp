/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Gui/GameMenu.h"

// The game's Escape key gate

using openblack::gui::EscapeBlocked;
using openblack::gui::EscapeState;

TEST(MenuEscape, PlainEscapeOpensTheMenu)
{
	EXPECT_FALSE(EscapeBlocked({}));
}

TEST(MenuEscape, ShiftOrCtrlBlocksAltDoesNot)
{
	EXPECT_TRUE(EscapeBlocked({.shiftOrCtrl = true}));
	// Alt is not in the gate's masks: the caller passes shiftOrCtrl false for it
	EXPECT_FALSE(EscapeBlocked({.shiftOrCtrl = false}));
}

TEST(MenuEscape, FilmWideScreenAndDebounce)
{
	EXPECT_TRUE(EscapeBlocked({.filmPlaying = true}));
	EXPECT_TRUE(EscapeBlocked({.scriptWideScreen = true}));
	EXPECT_TRUE(EscapeBlocked({.msSinceMenuClosed = 0}));
	EXPECT_TRUE(EscapeBlocked({.msSinceMenuClosed = 300}));
	EXPECT_FALSE(EscapeBlocked({.msSinceMenuClosed = 301}));
	EXPECT_FALSE(EscapeBlocked({.msSinceMenuClosed = -5})); // negative passes
}

TEST(MenuEscape, AnOpenMenuAlwaysTakesIt)
{
	EXPECT_FALSE(EscapeBlocked({.shiftOrCtrl = true, .filmPlaying = true, .menuOpen = true}));
}

TEST(MenuEscape, OverAQuestion)
{
	using openblack::gui::EscapeOverQuestion;
	using openblack::gui::EscapeOverQuestionResult;
	EXPECT_EQ(EscapeOverQuestion(true, false), EscapeOverQuestionResult::Answer); // a notice is answered
	EXPECT_EQ(EscapeOverQuestion(true, true), EscapeOverQuestionResult::Answer);
	EXPECT_EQ(EscapeOverQuestion(false, true), EscapeOverQuestionResult::Continue); // the main page's quit question
	EXPECT_EQ(EscapeOverQuestion(false, false), EscapeOverQuestionResult::Nothing); // the options' quit question
}

TEST(MenuEscape, PageAfterAnAnswer)
{
	using Page = openblack::gui::GameMenu::Page;
	using openblack::gui::GameMenu;
	EXPECT_EQ(GameMenu::PageAfterAnswer(Page::Players, Page::Options), Page::Players); // the kept tab
	EXPECT_EQ(GameMenu::PageAfterAnswer(std::nullopt, Page::Options), Page::Main);     // «No» on Options' quit question
	EXPECT_EQ(GameMenu::PageAfterAnswer(std::nullopt, Page::Main), std::nullopt);
	EXPECT_EQ(GameMenu::PageAfterAnswer(std::nullopt, Page::Advanced), std::nullopt);
}

TEST(MenuEscape, DimmedBehindAQuestion)
{
	using openblack::gui::GameMenu;
	EXPECT_EQ(GameMenu::AlphaBehindQuestion(1.0f, 0.0f), 1.0f);  // the question just opened
	EXPECT_EQ(GameMenu::AlphaBehindQuestion(1.0f, 1.0f), 0.25f); // held: a quarter
	EXPECT_EQ(GameMenu::AlphaBehindQuestion(0.5f, 1.0f), 0.0f);  // fading out: never below 0
}
