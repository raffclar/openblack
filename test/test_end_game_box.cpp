/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "Gui/EndGameBoxRules.h"

using namespace openblack;
using namespace openblack::end_game_box;
using game_end::Event;

namespace
{
constexpr auto k_One = PlayerNames::PLAYER_ONE;
constexpr auto k_Two = PlayerNames::PLAYER_TWO;
constexpr auto k_Three = PlayerNames::PLAYER_THREE;
constexpr auto k_Seven = PlayerNames::PLAYER_SEVEN;

const Titles k_Titles {
    .leave = u"Are you sure you want to quit this game?",
    .lost = u"You have lost the game.",
    .won = u"Congratulations! You've won the game!",
    .gameOver = u"Game Over",
    .winnerFormat = u"%s - %s Won",
};
} // namespace

// The god of this computer is out while two gods are still in: the player can watch, and the game goes on
TEST(EndGameBox, TheLocalGodOutWhileTheGameGoesOnCanWatch)
{
	State state;
	const std::array events {Event {.kind = Event::Kind::GodOut, .god = k_One}};
	const auto opened = Follow(events, k_One, state);
	ASSERT_TRUE(opened.endBox.has_value());
	EXPECT_EQ(opened.endBox->kind, Kind::Lost);
	EXPECT_TRUE(opened.endBox->watch);
	EXPECT_FALSE(StopsTheGame(*opened.endBox, state));
}

// The god of this computer is the last to go out: the game is decided, and the box stops the turns
TEST(EndGameBox, TheLocalGodOutAndDecidedStopsTheGame)
{
	State state;
	const std::array events {Event {.kind = Event::Kind::GodOutAndDecided, .god = k_Two}};
	const auto opened = Follow(events, k_Two, state);
	ASSERT_TRUE(opened.endBox.has_value());
	EXPECT_EQ(opened.endBox->kind, Kind::LostAndDecided);
	EXPECT_FALSE(opened.endBox->watch);
	EXPECT_TRUE(StopsTheGame(*opened.endBox, state));
}

// Another god is the last to go out: the player of this computer won, and the box stops the turns
TEST(EndGameBox, AnotherGodOutAndDecidedIsWon)
{
	State state;
	const std::array events {Event {.kind = Event::Kind::GodOutAndDecided, .god = k_Two}};
	const auto opened = Follow(events, k_One, state);
	ASSERT_TRUE(opened.endBox.has_value());
	EXPECT_EQ(opened.endBox->kind, Kind::Won);
	EXPECT_TRUE(StopsTheGame(*opened.endBox, state));
}

// Vanilla-bugs 469: a player who lost and watches is told that the game is won, and the box does not stop the turns
TEST(EndGameBox, APlayerWhoLostAndWatchesIsToldTheGameIsWon)
{
	State state;
	const std::array first {Event {.kind = Event::Kind::GodOut, .god = k_One}};
	std::ignore = Follow(first, k_One, state);
	const std::array last {Event {.kind = Event::Kind::GodOutAndDecided, .god = k_Two}};
	const auto opened = Follow(last, k_One, state);
	ASSERT_TRUE(opened.endBox.has_value());
	EXPECT_EQ(opened.endBox->kind, Kind::Won);
	EXPECT_FALSE(opened.endBox->watch);
	EXPECT_FALSE(StopsTheGame(*opened.endBox, state));
}

// Another god is out and the game goes on: the box with OK names the god
TEST(EndGameBox, AnotherGodOutOpensTheBoxWithOk)
{
	State state;
	const std::array events {Event {.kind = Event::Kind::GodOut, .god = k_Three}};
	const auto opened = Follow(events, k_One, state);
	EXPECT_FALSE(opened.endBox.has_value());
	EXPECT_EQ(opened.godOut, k_Three);
}

// While the end box shows itself, no box with OK opens. After the player closes it, the box with OK opens again.
TEST(EndGameBox, NoBoxWithOkWhileTheEndBoxShows)
{
	State state;
	const std::array first {Event {.kind = Event::Kind::GodOut, .god = k_One}};
	std::ignore = Follow(first, k_One, state);
	const std::array next {Event {.kind = Event::Kind::GodOut, .god = k_Three}};
	EXPECT_FALSE(Follow(next, k_One, state).godOut.has_value());
	StoppedShowing(state);
	EXPECT_EQ(Follow(next, k_One, state).godOut, k_Three);
}

// The question to leave is an end box: the game goes on for the player, and no box with OK opens while it shows
TEST(EndGameBox, TheQuestionToLeaveIsAnEndBoxOfItsOwn)
{
	State state;
	const auto box = AskToLeave(state);
	EXPECT_EQ(box.kind, Kind::LeaveQuestion);
	EXPECT_FALSE(box.watch);
	EXPECT_TRUE(state.gameGoesOn);
	EXPECT_FALSE(StopsTheGame(box, state));
	const std::array out {Event {.kind = Event::Kind::GodOut, .god = k_Three}};
	EXPECT_FALSE(Follow(out, k_One, state).godOut.has_value());
	// A player who asked to leave and went back does not stop the turns when the game is decided later
	StoppedShowing(state);
	const std::array decided {Event {.kind = Event::Kind::GodOutAndDecided, .god = k_Two}};
	const auto opened = Follow(decided, k_One, state);
	ASSERT_TRUE(opened.endBox.has_value());
	EXPECT_EQ(opened.endBox->kind, Kind::Won);
	EXPECT_FALSE(StopsTheGame(*opened.endBox, state));
}

// An end box later in the same turn closes the box with OK
TEST(EndGameBox, AnEndBoxClosesTheBoxWithOk)
{
	State state;
	const std::array events {Event {.kind = Event::Kind::GodOut, .god = k_Three},
	                         Event {.kind = Event::Kind::GodOut, .god = k_One}};
	const auto opened = Follow(events, k_One, state);
	EXPECT_FALSE(opened.godOut.has_value());
	ASSERT_TRUE(opened.endBox.has_value());
	EXPECT_EQ(opened.endBox->kind, Kind::Lost);
}

// A game decided by its conditions: the winner won, each other god among the first four lost, a looker-on sees who won
TEST(EndGameBox, TheConditionsDecideForEachGod)
{
	const std::array events {Event {.kind = Event::Kind::Won, .god = k_Two}};
	State winner;
	EXPECT_EQ(Follow(events, k_Two, winner).endBox->kind, Kind::Won);
	State loser;
	const auto lost = Follow(events, k_One, loser).endBox;
	EXPECT_EQ(lost->kind, Kind::LostAndDecided);
	EXPECT_TRUE(StopsTheGame(*lost, loser));
	State onlooker;
	EXPECT_EQ(Follow(events, k_Seven, onlooker).endBox->kind, Kind::Won);
}

// The titles of the end box, and the looker-on's title with the winner's name
TEST(EndGameBox, TheTitlesAreTheGamesTexts)
{
	EXPECT_EQ(Title(Kind::Won, false, u"", k_Titles), u"Congratulations! You've won the game!");
	EXPECT_EQ(Title(Kind::Lost, false, u"", k_Titles), u"You have lost the game.");
	EXPECT_EQ(Title(Kind::LostAndDecided, false, u"", k_Titles), u"You have lost the game.");
	EXPECT_EQ(Title(Kind::LeaveQuestion, false, u"", k_Titles), u"Are you sure you want to quit this game?");
	EXPECT_EQ(Title(Kind::Won, true, u"Bob", k_Titles), u"Game Over - Bob Won");
}

// A god that a human plays has the human's name; another god is "Player[n]"
TEST(EndGameBox, TheNamesOfTheGods)
{
	EXPECT_EQ(GodName(k_Two, std::u16string_view(u"Alice")), u"Alice");
	EXPECT_EQ(GodName(k_Two, std::nullopt), u"Player[2]");
	EXPECT_EQ(GodName(k_Seven, std::u16string_view(u"")), u"Player[7]");
	const std::array<std::u16string_view, 1> name {u"Player[2]"};
	EXPECT_EQ(Format(u"%s is out of the game.", name), u"Player[2] is out of the game.");
}
