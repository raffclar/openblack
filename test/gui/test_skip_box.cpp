/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The start-of-game question a returning player is asked, and what each answer skips

#include <string>
#include <utility>
#include <vector>

#include <Gui/SkipBox.h>
#include <Gui/TextDatabase.h>
#include <Story/NewGameChoice.h>
#include <gtest/gtest.h>

#include "FakeGameFont.h"

using namespace openblack::gui;
using namespace openblack::gui::test;
using openblack::new_game_choice::AsksAtNewGame;
using openblack::new_game_choice::Choice;
using openblack::new_game_choice::ClampChoice;
using openblack::new_game_choice::NewGameStart;
using openblack::new_game_choice::ParseNewGameStart;
using openblack::new_game_choice::SkipFor;
using openblack::new_game_choice::TutorialSkip;

namespace
{
TextDatabase SkipTexts()
{
	std::u16string script;
	const std::vector<std::pair<std::u16string, std::u16string>> texts = {
	    {u"HELP_TEXT_PATCH_NONE", u"BAD"},
	    {u"HELP_TEXT_NONE", u"BAD"},
	    {u"HELP_TEXT_PATCH_04", u"AB AB"},
	    {u"HELP_TEXT_PATCH_05", u"A"},
	    {u"HELP_TEXT_PATCH_06", u"B"},
	    {u"HELP_TEXT_PATCH_07", u"AB"},
	    {u"HELP_TEXT_REQUESTER_BOXES_04", u"AW"},
	};
	for (const auto& [name, text] : texts)
	{
		script.append(u"ADD_TEXT(1, N, \"").append(name).append(u"\", \"").append(text).append(u"\")\n");
	}
	TextDatabase database;
	database.AddScript(Utf16Script(script));
	return database;
}

struct SkipFixture
{
	TextDatabase texts = SkipTexts();
	GameFont font = LoadFont();
	SkipBox box {texts, font, DialogPainter::k_MidTextSize};

	SkipFixture() { box.Show(); }

	SkipBox::MouseUpResult Click(glm::ivec2 point)
	{
		box.MouseMove(point);
		box.MouseDown(point);
		return box.MouseUp(point);
	}
};

/// The middle of the OK button
constexpr glm::ivec2 k_Ok = SkipBox::k_ButtonPosition + (SkipBox::k_ButtonSize / 2);
} // namespace

TEST(NewGameChoice, EachAnswerSkipsMoreThanTheOneBefore)
{
	EXPECT_EQ(SkipFor(Choice::StartNormally), TutorialSkip {});
	EXPECT_EQ(SkipFor(Choice::SkipToCreatureSelect), (TutorialSkip {.skipTutorial = true}));
	EXPECT_EQ(SkipFor(Choice::SkipCreatureTutorial), (TutorialSkip {.skipTutorial = true, .skipCreatureTraining = true}));
	EXPECT_EQ(SkipFor(Choice::KeepOldCreature),
	          (TutorialSkip {.skipTutorial = true, .skipCreatureTraining = true, .keepOldCreature = true}));
}

TEST(NewGameChoice, OnlyAReturningPlayerIsAsked)
{
	EXPECT_FALSE(AsksAtNewGame(0, false));
	EXPECT_FALSE(AsksAtNewGame(1, false));
	EXPECT_TRUE(AsksAtNewGame(1, true));
	EXPECT_TRUE(AsksAtNewGame(2, false));
	EXPECT_TRUE(AsksAtNewGame(0, true));
}

TEST(NewGameChoice, SelectionsAreBroughtIntoRange)
{
	EXPECT_EQ(ClampChoice(-1), Choice::StartNormally);
	EXPECT_EQ(ClampChoice(2), Choice::SkipCreatureTutorial);
	EXPECT_EQ(ClampChoice(7), Choice::KeepOldCreature);
}

// The developers' start answers the question with one of its own answers, or asks it
TEST(NewGameChoice, DevelopersStartNamesTheQuestionsAnswers)
{
	EXPECT_EQ(ParseNewGameStart("creature"), NewGameStart {.answer = Choice::SkipToCreatureSelect});
	EXPECT_EQ(ParseNewGameStart("story"), NewGameStart {.answer = Choice::SkipCreatureTutorial});
	EXPECT_EQ(ParseNewGameStart("old"), NewGameStart {.answer = Choice::KeepOldCreature});
	EXPECT_EQ(ParseNewGameStart("normal"), NewGameStart {.answer = Choice::StartNormally});
	EXPECT_EQ(ParseNewGameStart("ask"), NewGameStart {.ask = true});
	EXPECT_FALSE(ParseNewGameStart("beach").has_value());
	EXPECT_FALSE(ParseNewGameStart("").has_value());
	// Skipping to the creature select skips the opening only; the story's answer skips the creature's training too
	EXPECT_EQ(SkipFor(ParseNewGameStart("creature")->answer.value()),
	          (TutorialSkip {.skipTutorial = true, .skipCreatureTraining = false, .keepOldCreature = false}));
	EXPECT_EQ(SkipFor(ParseNewGameStart("story")->answer.value()),
	          (TutorialSkip {.skipTutorial = true, .skipCreatureTraining = true, .keepOldCreature = false}));
}

TEST(SkipBox, IsLaidOutAsTheOriginal)
{
	EXPECT_EQ(SkipBox::k_Box.min, glm::ivec2(200, 155));
	EXPECT_EQ(SkipBox::k_Box.max, glm::ivec2(600, 445));
	EXPECT_EQ(SkipBox::GetRadioRect(0).min, glm::ivec2(250, 225));
	EXPECT_EQ(SkipBox::GetRadioRect(3).min, glm::ivec2(250, 357));
	EXPECT_EQ(SkipBox::GetRadioRect(3).max, glm::ivec2(274, 381));
}

TEST(SkipBox, StartsNormallyUnlessAnotherAnswerIsPicked)
{
	SkipFixture f;
	EXPECT_TRUE(f.box.IsActive());
	EXPECT_EQ(f.box.GetSelection(), Choice::StartNormally);
	const auto picked = f.Click(SkipBox::GetRadioRect(2).Centre());
	EXPECT_TRUE(picked.clicked);
	EXPECT_FALSE(picked.answer.has_value());
	EXPECT_EQ(f.box.GetSelection(), Choice::SkipCreatureTutorial);
	// Picking it again keeps it picked
	f.Click(SkipBox::GetRadioRect(2).Centre());
	EXPECT_EQ(f.box.GetSelection(), Choice::SkipCreatureTutorial);

	const auto answered = f.Click(k_Ok);
	EXPECT_TRUE(answered.clicked);
	ASSERT_TRUE(answered.answer.has_value());
	EXPECT_EQ(*answered.answer, Choice::SkipCreatureTutorial);
	EXPECT_FALSE(f.box.IsActive());
}

TEST(SkipBox, RadioButtonsAreHitInTheirCircle)
{
	SkipFixture f;
	// The square's corner is outside the circle
	const auto corner = f.Click(SkipBox::GetRadioRect(1).min + 1);
	EXPECT_FALSE(corner.clicked);
	EXPECT_EQ(f.box.GetSelection(), Choice::StartNormally);
	f.Click(SkipBox::GetRadioRect(1).Centre() + glm::ivec2(0, 11));
	EXPECT_EQ(f.box.GetSelection(), Choice::SkipToCreatureSelect);
}

TEST(SkipBox, TheTextsClickButDoNothing)
{
	SkipFixture f;
	const auto text = f.Click(SkipBox::k_FirstAnswer + glm::ivec2(100, 44 * 3 + 5));
	EXPECT_TRUE(text.clicked);
	EXPECT_FALSE(text.answer.has_value());
	EXPECT_EQ(f.box.GetSelection(), Choice::StartNormally);
	EXPECT_TRUE(f.Click(SkipBox::k_Question.Centre()).clicked);
	// The box's empty space doesn't click
	EXPECT_FALSE(f.Click({210, 430}).clicked);
	EXPECT_TRUE(f.box.IsActive());
}

TEST(SkipBox, TheButtonsLabelAnswersToo)
{
	SkipFixture f;
	// "AW" at the mid text size, from the button's right edge
	const auto label = f.Click({SkipBox::k_ButtonPosition.x + SkipBox::k_ButtonSize + 10, k_Ok.y});
	EXPECT_TRUE(label.answer.has_value());
}

TEST(SkipBox, AnswersOnlyWhenLetGoOverTheButton)
{
	SkipFixture f;
	f.box.MouseMove(k_Ok);
	f.box.MouseDown(k_Ok);
	f.box.MouseMove({400, 300});
	EXPECT_FALSE(f.box.MouseUp({400, 300}).answer.has_value());
	// A button already held as the box came up isn't a click
	f.box.Show();
	EXPECT_FALSE(f.box.MouseUp(k_Ok).clicked);
	EXPECT_TRUE(f.box.IsActive());
}

TEST(SkipBox, FadesInAndOutAndKeepsTheAnswerPicked)
{
	SkipFixture f;
	f.box.Update(0.6f);
	f.Click(SkipBox::GetRadioRect(3).Centre());
	f.Click(k_Ok);
	EXPECT_TRUE(f.box.IsVisible());
	// Fading out it takes no clicks
	EXPECT_FALSE(f.Click(SkipBox::GetRadioRect(0).Centre()).clicked);
	f.box.Update(0.1f);
	EXPECT_TRUE(f.box.IsVisible());
	f.box.Update(0.11f);
	EXPECT_FALSE(f.box.IsVisible());

	f.box.Show();
	EXPECT_EQ(f.box.GetSelection(), Choice::KeepOldCreature);
}
