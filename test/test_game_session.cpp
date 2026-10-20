/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "GameSession.h"

using namespace openblack::game_session;

namespace
{
ExitSaveFacts PlainQuit()
{
	return {.end = End::Quit, .currentProfile = u"Ann"};
}
} // namespace

TEST(GameSession, EachEndGivesTheStartOfTheNextGame)
{
	EXPECT_EQ(NextStart(End::NewGame), Start::NewGame);
	EXPECT_EQ(NextStart(End::Continue), Start::Continue);
	EXPECT_EQ(NextStart(End::Online), Start::Online);
	EXPECT_EQ(NextStart(End::SavedGame), Start::SavedGame);
	EXPECT_EQ(NextStart(End::Skirmish), Start::Skirmish);
	EXPECT_FALSE(NextStart(End::Quit).has_value());
}

TEST(GameSession, APlainStartContinuesAndAFirstStartIsANewGame)
{
	EXPECT_EQ(FirstStart({.hadProfile = true}), Start::Continue);
	EXPECT_EQ(FirstStart({.hadProfile = false}), Start::NewGame);
	// The new game option wins, as it does on the command line of the game
	EXPECT_EQ(FirstStart({.hadProfile = true, .newGameAsked = true}), Start::NewGame);
	EXPECT_EQ(FirstStart({.hadProfile = true, .newGameAsked = true, .continueAsked = true}), Start::NewGame);
}

TEST(GameSession, ADevelopersRunStartsANewGameUnlessItAsksToContinue)
{
	EXPECT_EQ(FirstStart({.hadProfile = true, .developerRun = true}), Start::NewGame);
	EXPECT_EQ(FirstStart({.hadProfile = true, .developerRun = true, .continueAsked = true}), Start::Continue);
	EXPECT_EQ(FirstStart({.hadProfile = false, .developerRun = true, .continueAsked = true}), Start::NewGame);
}

TEST(GameSession, AContinueWithNoSaveOnExitStartsANewGame)
{
	EXPECT_EQ(Resolve(Start::Continue, true), Start::Continue);
	EXPECT_EQ(Resolve(Start::Continue, false), Start::NewGame);
	EXPECT_EQ(Resolve(Start::NewGame, true), Start::NewGame);
	EXPECT_EQ(Resolve(Start::SavedGame, false), Start::SavedGame);
}

TEST(GameSession, OnlyAContinuedGameShowsTheMenuAsItStarts)
{
	EXPECT_TRUE(ShowsMenuOnStart(Start::Continue, true));
	// The load did not happen: the new game that takes its place shows no menu
	EXPECT_FALSE(ShowsMenuOnStart(Start::Continue, false));
	EXPECT_FALSE(ShowsMenuOnStart(Start::NewGame, true));
	EXPECT_FALSE(ShowsMenuOnStart(Start::SavedGame, true));
}

TEST(GameSession, AWindowCloseWaitsForTheEndOfAScriptsCutScene)
{
	EXPECT_TRUE(CloseEndsGame(true, false));
	// The game continues until the cut scene ends, and keeps the close until then
	EXPECT_FALSE(CloseEndsGame(true, true));
	EXPECT_FALSE(CloseEndsGame(false, false));
	EXPECT_FALSE(CloseEndsGame(false, true));
}

TEST(GameSession, AGameSavesAsItEndsIntoTheProfilePlayed)
{
	EXPECT_EQ(ExitSaveFor(PlainQuit()), ExitSave::Current);
	auto facts = PlainQuit();
	facts.end = End::NewGame;
	EXPECT_EQ(ExitSaveFor(facts), ExitSave::Current);
	facts.end = End::Continue;
	EXPECT_EQ(ExitSaveFor(facts), ExitSave::Current);
	// An empty previous name is no previous profile
	facts.previousProfile = u"";
	EXPECT_EQ(ExitSaveFor(facts), ExitSave::Current);
}

TEST(GameSession, SomeGamesEndWithNoSave)
{
	for (auto change : {+[](ExitSaveFacts& f) { f.gameOver = true; }, +[](ExitSaveFacts& f) { f.multiplayer = true; },
	                    +[](ExitSaveFacts& f) { f.skirmish = true; }, +[](ExitSaveFacts& f) { f.godsPlayground = true; },
	                    +[](ExitSaveFacts& f) { f.end = End::SavedGame; }})
	{
		auto facts = PlainQuit();
		change(facts);
		EXPECT_EQ(ExitSaveFor(facts), ExitSave::None);
	}
}

TEST(GameSession, TheSaveGoesToThePreviousProfileWhenThePlayerPickedAnother)
{
	auto facts = PlainQuit();
	facts.end = End::Continue;
	facts.currentProfile = u"Bob";
	facts.previousProfile = u"Ann";
	facts.previousExists = true;
	EXPECT_EQ(ExitSaveFor(facts), ExitSave::Previous);
	facts.end = End::NewGame;
	EXPECT_EQ(ExitSaveFor(facts), ExitSave::Previous);
	// The previous profile was deleted: no profile gets the save
	facts.previousExists = false;
	EXPECT_EQ(ExitSaveFor(facts), ExitSave::None);
}

TEST(GameSession, TheSamePreviousProfileSavesUnlessTheGameStartsAgain)
{
	auto facts = PlainQuit();
	facts.previousProfile = u"ANN";
	facts.previousExists = true;
	EXPECT_EQ(ExitSaveFor(facts), ExitSave::Current);
	facts.end = End::NewGame;
	EXPECT_EQ(ExitSaveFor(facts), ExitSave::Current);
	// A restart as the same profile has no save: the next game continues from the last save on exit
	facts.end = End::Continue;
	EXPECT_EQ(ExitSaveFor(facts), ExitSave::None);
}

TEST(GameSession, ProfileNamesCompareWithoutTheCaseOfPlainLetters)
{
	EXPECT_TRUE(SameProfileName(u"Ann", u"aNN"));
	EXPECT_FALSE(SameProfileName(u"Ann", u"Anne"));
}
