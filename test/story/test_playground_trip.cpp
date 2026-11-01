/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <initializer_list>

#include <gtest/gtest.h>

#include "Gui/EscapeKeyRules.h"
#include "Story/PlaygroundTrip.h"

using namespace openblack::playground_trip;

TEST(PlaygroundTrip, F2AsksOnAStoryLand)
{
	EXPECT_TRUE(MayAskToGo({.land = 1}));
	EXPECT_TRUE(MayAskToGo({.land = 5}));
}

TEST(PlaygroundTrip, F2DoesNothingInASkirmish)
{
	EXPECT_FALSE(MayAskToGo({.skirmish = true, .land = 1}));
}

TEST(PlaygroundTrip, F2DoesNothingInANetworkGame)
{
	EXPECT_FALSE(MayAskToGo({.networkGame = true, .land = 1}));
}

TEST(PlaygroundTrip, F2DoesNothingInTheTemple)
{
	EXPECT_FALSE(MayAskToGo({.inTemple = true, .land = 1}));
}

TEST(PlaygroundTrip, F2DoesNothingOnThePlayground)
{
	EXPECT_FALSE(MayAskToGo({.land = k_PlaygroundLand}));
}

TEST(PlaygroundTrip, F2DoesNothingWhileABoxIsUp)
{
	EXPECT_FALSE(MayAskToGo({.land = 1, .boxUp = true}));
}

TEST(PlaygroundTrip, F2DoesNothingInAScriptsCutScene)
{
	EXPECT_FALSE(MayAskToGo({.land = 1, .scriptHoldsCinemaBars = true}));
}

TEST(PlaygroundTrip, ThePlaygroundIsTheLandOfEscape)
{
	EXPECT_EQ(k_PlaygroundLand, openblack::escape_key::k_GodsPlaygroundLand);
}

TEST(PlaygroundTrip, NoLessonRunsAtFirst)
{
	Lessons lessons;
	EXPECT_FALSE(lessons.running);
	EXPECT_FALSE(AskEnd(lessons));
	EXPECT_FALSE(lessons.endAsked);
	EXPECT_FALSE(TakeEnd(lessons));
}

TEST(PlaygroundTrip, EscapeEndsTheLessonAtTheEndOfTheTurn)
{
	Lessons lessons;
	StartLesson(lessons);
	EXPECT_TRUE(lessons.running);
	EXPECT_TRUE(AskEnd(lessons));
	// The lesson runs until the end of the turn
	EXPECT_TRUE(lessons.running);
	EXPECT_TRUE(TakeEnd(lessons));
	EXPECT_FALSE(lessons.running);
	EXPECT_FALSE(lessons.endAsked);
	// The end comes once
	EXPECT_FALSE(TakeEnd(lessons));
}

TEST(PlaygroundTrip, TheNextLessonRunsAgain)
{
	Lessons lessons;
	StartLesson(lessons);
	ASSERT_TRUE(AskEnd(lessons));
	ASSERT_TRUE(TakeEnd(lessons));
	StartLesson(lessons);
	EXPECT_TRUE(lessons.running);
	EXPECT_FALSE(lessons.endAsked);
}

TEST(PlaygroundTrip, LeavingStopsTheLessonAndItsEnd)
{
	Lessons lessons;
	StartLesson(lessons);
	ASSERT_TRUE(AskEnd(lessons));
	Leave(lessons);
	EXPECT_FALSE(lessons.running);
	EXPECT_FALSE(TakeEnd(lessons));
}

TEST(PlaygroundTrip, F2AsksOverTheMainMenu)
{
	EXPECT_TRUE(MenuLetsAsk({.box = MenuBox::Main}));
	// The question to quit puts the main menu on hold
	EXPECT_FALSE(MenuLetsAsk({.box = MenuBox::Main, .questionUp = true}));
}

TEST(PlaygroundTrip, F2AsksOverEachPageThatEscapeCanClose)
{
	for (const auto box :
	     {MenuBox::Options, MenuBox::Advanced, MenuBox::Players, MenuBox::Controls, MenuBox::Statistics, MenuBox::Conditions})
	{
		EXPECT_TRUE(MenuLetsAsk({.box = box}));
		// A key that the Controls page waits for, or a question on any page, stops F2
		EXPECT_FALSE(MenuLetsAsk({.box = box, .questionUp = true}));
	}
}

TEST(PlaygroundTrip, F2AsksOverTheLandInformationBox)
{
	EXPECT_FALSE(OtherBoxRefuses({}));
	EXPECT_FALSE(OtherBoxRefuses({.landInfoBox = true}));
	// The menu has its own rule
	EXPECT_FALSE(OtherBoxRefuses({.menu = true, .landInfoBox = true}));
	// Each other box that takes the keyboard stops F2, also over the land information box
	EXPECT_TRUE(OtherBoxRefuses({.skirmishBox = true}));
	EXPECT_TRUE(OtherBoxRefuses({.landInfoBox = true, .saveGameBoxes = true}));
	EXPECT_TRUE(OtherBoxRefuses({.tattooEditor = true}));
	// The story history and the music controls let the keys go to the game
	EXPECT_FALSE(OtherBoxRefuses({.storyHistory = true, .musicControls = true}));
}

TEST(PlaygroundTrip, AnUntoldChangeOfTheDetailStopsF2OnTheOptionsPages)
{
	EXPECT_FALSE(MenuLetsAsk({.box = MenuBox::Options, .detailUntold = true}));
	EXPECT_FALSE(MenuLetsAsk({.box = MenuBox::Advanced, .detailUntold = true}));
	// The other pages do not read the detail
	EXPECT_TRUE(MenuLetsAsk({.box = MenuBox::Main, .detailUntold = true}));
	EXPECT_TRUE(MenuLetsAsk({.box = MenuBox::Controls, .detailUntold = true}));
}

TEST(PlaygroundTrip, ARestartThatWaitsStopsF2OnThePlayersPage)
{
	EXPECT_FALSE(MenuLetsAsk({.box = MenuBox::Players, .restartPending = true}));
	EXPECT_TRUE(MenuLetsAsk({.box = MenuBox::Options, .restartPending = true}));
}

TEST(PlaygroundTrip, OnlyTheMainMenuPausesWhenItComesBack)
{
	EXPECT_TRUE(PausesWhenBack(MenuBox::Main));
	for (const auto box :
	     {MenuBox::Options, MenuBox::Advanced, MenuBox::Players, MenuBox::Controls, MenuBox::Statistics, MenuBox::Conditions})
	{
		EXPECT_FALSE(PausesWhenBack(box));
	}
}
