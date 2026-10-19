/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The message that tells the player that a save did not write its world files, in the box of the save game room

#include <string>
#include <utility>
#include <vector>

#include <Gui/SaveGameBoxes.h>
#include <Gui/TextDatabase.h>
#include <SDL_keycode.h>
#include <gtest/gtest.h>

#include "FakeGameFont.h"

using namespace openblack::gui;
using namespace openblack::gui::test;

namespace
{
TextDatabase BoxTexts()
{
	std::u16string script;
	const std::vector<std::pair<std::u16string, std::u16string>> texts = {
	    {u"HELP_TEXT_NONE", u"BAD"},
	    {u"HELP_TEXT_REQUESTER_BOXES_01", u"A"},
	    {u"HELP_TEXT_REQUESTER_BOXES_02", u"B"},
	    {u"HELP_TEXT_REQUESTER_BOXES_04", u"AW"},
	    {u"HELP_TEXT_PAUSE_STATS_145", u"YES"},
	    {u"HELP_TEXT_PAUSE_STATS_144", u"NO"},
	};
	for (const auto& [name, text] : texts)
	{
		script.append(u"ADD_TEXT(1, N, \"").append(name).append(u"\", \"").append(text).append(u"\")\n");
	}
	TextDatabase database;
	database.AddScript(Utf16Script(script));
	return database;
}

struct BoxFixture
{
	TextDatabase texts = BoxTexts();
	GameFont font = LoadFont();
	SaveGameBoxes boxes {texts, font};

	BoxFixture()
	{
		boxes.Tell(u"An error has occurred!");
		boxes.Update(1.0f);
	}

	bool Click(glm::ivec2 point)
	{
		boxes.MouseMove(point);
		boxes.MouseDown(point);
		return boxes.MouseUp(point);
	}
};

glm::ivec2 Middle(DialogRect rect)
{
	return (rect.min + rect.max) / 2;
}
} // namespace

TEST(SaveProblemBox, TheMessageStaysUntilItsOk)
{
	BoxFixture fixture;
	EXPECT_TRUE(fixture.boxes.IsActive());
	EXPECT_EQ(fixture.boxes.GetQuestion(), u"An error has occurred!");
	// The first letters of yes and no do nothing to a message
	fixture.boxes.TextInput(u"y");
	fixture.boxes.TextInput(u"n");
	EXPECT_TRUE(fixture.boxes.IsActive());
	EXPECT_FALSE(fixture.boxes.TakeAnswer().has_value());
	// Its one button is OK
	EXPECT_TRUE(fixture.Click(Middle(SaveGameBoxes::k_MessageOk)));
	EXPECT_FALSE(fixture.boxes.IsActive());
	EXPECT_EQ(fixture.boxes.TakeAnswer(), SaveGameBoxes::Answer::Ok);
}

TEST(SaveProblemBox, ReturnAndEscapeAreOk)
{
	for (const int key : {SDLK_RETURN, SDLK_KP_ENTER, SDLK_ESCAPE})
	{
		BoxFixture fixture;
		fixture.boxes.KeyDown(key);
		EXPECT_FALSE(fixture.boxes.IsActive()) << key;
		EXPECT_EQ(fixture.boxes.TakeAnswer(), SaveGameBoxes::Answer::Ok) << key;
	}
}

TEST(SaveProblemBox, AQuestionStillTakesNeitherReturnNorEscape)
{
	BoxFixture fixture;
	fixture.boxes.Ask(u"Are you sure you want to load this game?");
	fixture.boxes.KeyDown(SDLK_RETURN);
	fixture.boxes.KeyDown(SDLK_ESCAPE);
	EXPECT_TRUE(fixture.boxes.IsActive());
	fixture.boxes.TextInput(u"y");
	EXPECT_EQ(fixture.boxes.TakeAnswer(), SaveGameBoxes::Answer::Yes);
}
