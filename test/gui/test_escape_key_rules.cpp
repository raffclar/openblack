/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <chrono>
#include <optional>
#include <string_view>

#include <gtest/gtest.h>

#include "Gui/EscapeKeyRules.h"

using namespace openblack::escape_key;
using std::chrono::milliseconds;

TEST(EscapeKeyRules, OpensTheMenuInPlay)
{
	const auto response = Respond({});
	EXPECT_EQ(response.outcome, Outcome::OpenMenu);
	EXPECT_FALSE(response.stopHelpScripts);
	EXPECT_FALSE(response.clearScreenFade);
}

TEST(EscapeKeyRules, DoesNothingInAScriptsCutScene)
{
	const auto response = Respond({.scriptHoldsCinemaBars = true, .helpScriptHoldsDialogue = true});
	EXPECT_EQ(response.outcome, Outcome::Nothing);
	EXPECT_FALSE(response.stopHelpScripts);
}

TEST(EscapeKeyRules, DoesNothingWithShiftOrCtrl)
{
	const auto response = Respond({.shiftOrCtrl = true, .helpScriptHoldsDialogue = true});
	EXPECT_EQ(response.outcome, Outcome::Nothing);
	EXPECT_FALSE(response.stopHelpScripts);
}

TEST(EscapeKeyRules, StopsAHelpScriptHoldingTheDialogue)
{
	const auto response = Respond({.helpScriptHoldsDialogue = true});
	EXPECT_EQ(response.outcome, Outcome::OpenMenu);
	EXPECT_TRUE(response.stopHelpScripts);
}

TEST(EscapeKeyRules, TheMenuWaitsThreeHundredMillisecondsAfterClosing)
{
	EXPECT_TRUE(MenuMayOpen(std::nullopt));
	EXPECT_FALSE(MenuMayOpen(milliseconds {0}));
	EXPECT_FALSE(MenuMayOpen(milliseconds {300}));
	EXPECT_TRUE(MenuMayOpen(milliseconds {301}));

	// The help script stops even when the menu stays shut
	const auto response = Respond({.helpScriptHoldsDialogue = true, .sinceMenuClosed = milliseconds {120}});
	EXPECT_EQ(response.outcome, Outcome::Nothing);
	EXPECT_TRUE(response.stopHelpScripts);
}

TEST(EscapeKeyRules, AsksToLeaveTheGodsPlayground)
{
	const auto response = Respond({.shiftOrCtrl = true, .helpScriptHoldsDialogue = true, .inGodsPlayground = true});
	EXPECT_EQ(response.outcome, Outcome::AskLeavePlayground);
	EXPECT_FALSE(response.stopHelpScripts);
	EXPECT_TRUE(response.clearScreenFade);
}

TEST(EscapeKeyRules, EndsAPlaygroundLesson)
{
	const auto response = Respond({.inGodsPlayground = true, .playgroundLessonRunning = true});
	EXPECT_EQ(response.outcome, Outcome::EndPlaygroundLesson);
	EXPECT_TRUE(response.clearScreenFade);
}

TEST(EscapeKeyRules, ThePlaygroundNeverOpensTheMenu)
{
	EXPECT_EQ(Respond({.scriptHoldsCinemaBars = true, .inGodsPlayground = true}).outcome, Outcome::Nothing);
	const auto fading = Respond({.inGodsPlayground = true, .screenClear = false});
	EXPECT_EQ(fading.outcome, Outcome::Nothing);
	EXPECT_FALSE(fading.clearScreenFade);
}

// The reason is empty only when Escape opens the menu, in all situations
TEST(EscapeKeyRules, GivesAReasonOnlyWhenTheMenuStaysShut)
{
	const std::array<std::optional<milliseconds>, 3> closedTimes {std::nullopt, milliseconds(100), milliseconds(400)};
	for (int bits = 0; bits < 16; ++bits)
	{
		for (const auto& sinceMenuClosed : closedTimes)
		{
			const Situation situation {
			    .shiftOrCtrl = (bits & 1) != 0,
			    .scriptHoldsCinemaBars = (bits & 2) != 0,
			    .helpScriptHoldsDialogue = (bits & 4) != 0,
			    .inGodsPlayground = (bits & 8) != 0,
			    .screenClear = true,
			    .playgroundLessonRunning = false,
			    .sinceMenuClosed = sinceMenuClosed,
			};
			const bool opens = Respond(situation).outcome == Outcome::OpenMenu;
			EXPECT_EQ(WhyMenuStaysShut(situation).empty(), opens) << bits;
		}
	}
	EXPECT_NE(WhyMenuStaysShut({.scriptHoldsCinemaBars = true}).find("cinema bars"), std::string_view::npos);
}

TEST(EscapeKeyRules, AsksAboutANewGameOnceTheGameIsOver)
{
	const auto response = Respond({.shiftOrCtrl = true, .helpScriptHoldsDialogue = true, .gameOver = true});
	EXPECT_EQ(response.outcome, Outcome::AskNewGame);
	EXPECT_FALSE(response.stopHelpScripts);
	EXPECT_FALSE(response.clearScreenFade);
	EXPECT_FALSE(WhyMenuStaysShut({.gameOver = true}).empty());
}

TEST(EscapeKeyRules, AsksAboutANewGameOnTheGodsPlaygroundToo)
{
	EXPECT_EQ(Respond({.inGodsPlayground = true, .gameOver = true}).outcome, Outcome::AskNewGame);
}

TEST(EscapeKeyRules, AsksNothingOnceTheGameIsOverInAScriptsCutScene)
{
	const auto response = Respond({.scriptHoldsCinemaBars = true, .helpScriptHoldsDialogue = true, .gameOver = true});
	EXPECT_EQ(response.outcome, Outcome::Nothing);
	EXPECT_FALSE(response.stopHelpScripts);
}

TEST(EscapeKeyRules, TheWaitStartsAgainWhenTheMainMenuStopsTakingTheInput)
{
	// The menu closes, or another page or a question shows over the main menu
	EXPECT_TRUE(MainMenuLeft({.wasActive = true, .active = false}));
	// The main menu stays, or it comes back
	EXPECT_FALSE(MainMenuLeft({.wasActive = true, .active = true}));
	EXPECT_FALSE(MainMenuLeft({.wasActive = false, .active = true}));
	EXPECT_FALSE(MainMenuLeft({}));
	// The tab of the main menu closes all the boxes and shows the main menu again
	EXPECT_TRUE(MainMenuLeft({.wasActive = true, .active = true, .shownAgain = true}));
}
