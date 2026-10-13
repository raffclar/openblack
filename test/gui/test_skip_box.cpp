/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The start-of-game question every new game asks, and what each answer skips

#include <optional>
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
using openblack::new_game_choice::AnswerAtOnce;
using openblack::new_game_choice::AvailableChoice;
using openblack::new_game_choice::Choice;
using openblack::new_game_choice::ClampChoice;
using openblack::new_game_choice::FirstLandStartsStory;
using openblack::new_game_choice::IsAvailable;
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

	/// A returning player's profile, with a creature to keep
	SkipFixture() { box.Show(true); }

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

// Without the developers' start answering it, the question is put at every new game, a new player's too
TEST(NewGameChoice, EveryNewGameIsAsked)
{
	for (const bool hasCreature : {false, true})
	{
		EXPECT_FALSE(AnswerAtOnce(std::nullopt, hasCreature).has_value());
		EXPECT_FALSE(AnswerAtOnce(NewGameStart {}, hasCreature).has_value());
		EXPECT_FALSE(AnswerAtOnce(NewGameStart {.ask = true}, hasCreature).has_value());
	}
}

// Keeping the old creature needs one; nothing else depends on the profile
TEST(NewGameChoice, KeepingTheOldCreatureNeedsOne)
{
	EXPECT_TRUE(IsAvailable(Choice::StartNormally, false));
	EXPECT_TRUE(IsAvailable(Choice::SkipToCreatureSelect, false));
	EXPECT_TRUE(IsAvailable(Choice::SkipCreatureTutorial, false));
	EXPECT_FALSE(IsAvailable(Choice::KeepOldCreature, false));
	EXPECT_TRUE(IsAvailable(Choice::KeepOldCreature, true));
	EXPECT_EQ(AvailableChoice(Choice::KeepOldCreature, false), Choice::SkipCreatureTutorial);
	EXPECT_EQ(AvailableChoice(Choice::KeepOldCreature, true), Choice::KeepOldCreature);
	EXPECT_EQ(AvailableChoice(Choice::SkipToCreatureSelect, false), Choice::SkipToCreatureSelect);
	// So the scripts are never told to keep a creature there isn't
	for (int i = 0; i < 4; ++i)
	{
		EXPECT_FALSE(SkipFor(AvailableChoice(ClampChoice(i), false)).keepOldCreature);
	}
}

// The developers' start answers at once exactly as before; only keeping a creature the profile hasn't got becomes
// skipping all of the creature tutorial
TEST(NewGameChoice, TheDevelopersStartAnswersAtOnce)
{
	for (const bool hasCreature : {false, true})
	{
		EXPECT_EQ(AnswerAtOnce(ParseNewGameStart("normal"), hasCreature), Choice::StartNormally);
		EXPECT_EQ(AnswerAtOnce(ParseNewGameStart("creature"), hasCreature), Choice::SkipToCreatureSelect);
		EXPECT_EQ(AnswerAtOnce(ParseNewGameStart("story"), hasCreature), Choice::SkipCreatureTutorial);
	}
	EXPECT_EQ(AnswerAtOnce(ParseNewGameStart("old"), true), Choice::KeepOldCreature);
	EXPECT_EQ(AnswerAtOnce(ParseNewGameStart("old"), false), Choice::SkipCreatureTutorial);
}

// A story land started first begins the story and asks the question; a playground started first does neither
TEST(NewGameChoice, APlaygroundStartsNoStory)
{
	EXPECT_TRUE(FirstLandStartsStory(false));
	EXPECT_FALSE(FirstLandStartsStory(true));
}

TEST(NewGameChoice, SelectionsAreBroughtIntoRange)
{
	EXPECT_EQ(ClampChoice(-1), Choice::StartNormally);
	EXPECT_EQ(ClampChoice(2), Choice::SkipCreatureTutorial);
	EXPECT_EQ(ClampChoice(7), Choice::DebugStartWithCow);
}

// The debug quickstart is always on offer, skips the creature choice and gives the player the Cow
TEST(NewGameChoice, DebugStartWithCowIsAlwaysAvailableAndMapsToTheCow)
{
	for (const bool hasCreature : {false, true})
	{
		EXPECT_TRUE(IsAvailable(Choice::DebugStartWithCow, hasCreature));
		EXPECT_EQ(AvailableChoice(Choice::DebugStartWithCow, hasCreature), Choice::DebugStartWithCow);
		EXPECT_EQ(AnswerAtOnce(ParseNewGameStart("cow"), hasCreature), Choice::DebugStartWithCow);
	}
	EXPECT_EQ(ClampChoice(4), Choice::DebugStartWithCow);
	EXPECT_EQ(
	    SkipFor(Choice::DebugStartWithCow),
	    (TutorialSkip {.skipTutorial = true, .skipCreatureTraining = true, .keepOldCreature = true, .startWithCow = true}));
	EXPECT_FALSE(SkipFor(Choice::KeepOldCreature).startWithCow);
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
	EXPECT_EQ(SkipBox::k_Box.min, glm::ivec2(200, 133));
	EXPECT_EQ(SkipBox::k_Box.max, glm::ivec2(600, 467));
	EXPECT_EQ(SkipBox::GetRadioRect(0).min, glm::ivec2(250, 203));
	EXPECT_EQ(SkipBox::GetRadioRect(3).min, glm::ivec2(250, 335));
	EXPECT_EQ(SkipBox::GetRadioRect(3).max, glm::ivec2(274, 359));
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
	f.box.Show(true);
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

	f.box.Show(true);
	EXPECT_EQ(f.box.GetSelection(), Choice::KeepOldCreature);
}

// A profile with no creature isn't offered to keep one: its radio button and answer are hidden in their places and
// take no clicks, the other three answers staying where they are
TEST(SkipBox, KeepingTheOldCreatureIsHiddenWithoutOne)
{
	SkipFixture f;
	f.box.Show(false);
	EXPECT_TRUE(f.box.IsActive());
	EXPECT_TRUE(f.box.IsOffered(Choice::StartNormally));
	EXPECT_TRUE(f.box.IsOffered(Choice::SkipToCreatureSelect));
	EXPECT_TRUE(f.box.IsOffered(Choice::SkipCreatureTutorial));
	EXPECT_FALSE(f.box.IsOffered(Choice::KeepOldCreature));
	EXPECT_FALSE(f.Click(SkipBox::GetRadioRect(3).Centre()).clicked);
	EXPECT_FALSE(f.Click(SkipBox::k_FirstAnswer + glm::ivec2(100, 44 * 3 + 5)).clicked);
	EXPECT_EQ(f.box.GetSelection(), Choice::StartNormally);
	f.Click(SkipBox::GetRadioRect(2).Centre());
	EXPECT_EQ(f.box.GetSelection(), Choice::SkipCreatureTutorial);
	const auto answered = f.Click(k_Ok);
	ASSERT_TRUE(answered.answer.has_value());
	EXPECT_EQ(*answered.answer, Choice::SkipCreatureTutorial);

	// With a creature again it is offered once more
	f.box.Show(true);
	EXPECT_TRUE(f.box.IsOffered(Choice::KeepOldCreature));
	EXPECT_TRUE(f.Click(SkipBox::GetRadioRect(3).Centre()).clicked);
	EXPECT_EQ(f.box.GetSelection(), Choice::KeepOldCreature);
}

// Keeping the old creature picked last time, then a profile without one: the selection moves to what it stands for
TEST(SkipBox, AKeptPickTheProfileCantTakeMoves)
{
	SkipFixture f;
	f.Click(SkipBox::GetRadioRect(3).Centre());
	f.Click(k_Ok);
	f.box.Show(false);
	EXPECT_EQ(f.box.GetSelection(), Choice::SkipCreatureTutorial);
	const auto answered = f.Click(k_Ok);
	ASSERT_TRUE(answered.answer.has_value());
	EXPECT_FALSE(SkipFor(*answered.answer).keepOldCreature);
}

// A change of land closes the question with no answer. Then the box takes no input, and the hand can show again.
TEST(SkipBox, AWithdrawnQuestionGivesNoAnswer)
{
	SkipFixture f;
	ASSERT_TRUE(f.box.IsActive());
	EXPECT_TRUE(f.box.Withdraw());
	EXPECT_FALSE(f.box.IsActive());
	// A click on the OK button of a closed box gives no answer
	EXPECT_FALSE(f.Click(k_Ok).answer.has_value());
	// The box fades out
	f.box.Update(1.0f);
	EXPECT_FALSE(f.box.IsVisible());
	// A closed box stays closed
	EXPECT_FALSE(f.box.Withdraw());
	// The next new game asks the question again
	f.box.Show(true);
	EXPECT_TRUE(f.box.IsActive());
	EXPECT_TRUE(f.Click(k_Ok).answer.has_value());
}
