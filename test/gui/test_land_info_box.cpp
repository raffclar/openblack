/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The land information box of a skirmish land, the help texts of its lines, and the question of Leave Skirmish Game

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <Gui/GameMenu.h>
#include <Gui/LandInfoBox.h>
#include <Gui/TextDatabase.h>
#include <LandMessage.h>
#include <SDL_keycode.h>
#include <gtest/gtest.h>

#include "FakeGameFont.h"

using namespace openblack;
using namespace openblack::gui;
using namespace openblack::gui::test;

namespace
{
TextDatabase Texts()
{
	const std::vector<std::pair<std::u16string, std::u16string>> texts = {
	    {u"HELP_TEXT_DIALOG_ADDITION_62", u"AW"},   {u"HELP_TEXT_DIALOG_BACK", u"AB"},
	    {u"HELP_TEXT_DIALOG_CONTINUEGAME", u"AAA"}, {u"HELP_TEXT_DIALOG_ADDITION_169", u"BBB"},
	    {u"HELP_TEXT_DIALOG_ADDITION_170", u"WWW"}, {u"HELP_TEXT_DIALOG_JOINGONLINE", u"AWA"},
	    {u"HELP_TEXT_DIALOG_OPTIONS", u"BAB"},      {u"HELP_TEXT_DIALOG_QUIT", u"BWB"},
	    {u"HELP_TEXT_DIALOG_ADDITION_167", u"WAW"}, {u"HELP_TEXT_REQUESTER_BOXES_01", u"A"},
	    {u"HELP_TEXT_REQUESTER_BOXES_02", u"B"},
	};
	std::u16string script;
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
	TextDatabase texts = Texts();
	GameFont font = LoadFont();
	LandInfoBox box {texts, font};

	void Show(std::u16string title)
	{
		box.Show(std::move(title));
		// The box is now fully in view
		box.Update(1.0f);
	}

	bool Click(glm::ivec2 point)
	{
		box.MouseMove(point);
		box.MouseDown(point);
		return box.MouseUp(point);
	}

	[[nodiscard]] std::vector<std::u16string> Lines() const
	{
		std::vector<std::u16string> lines;
		std::ranges::transform(box.GetList().GetItems(), std::back_inserter(lines), &List::Item::text);
		return lines;
	}
};

constexpr glm::ivec2 k_Back = LandInfoBox::k_BackPosition + (LandInfoBox::k_ArrowSize / 2);
} // namespace

TEST(LandMessage, ANumberOfZeroShowsTheTextOfTheScriptLine)
{
	EXPECT_FALSE(land_message::HelpTextOf(0).has_value());
}

TEST(LandMessage, ThreeAndFourShowTheLinesForTwoAndThreeGods)
{
	EXPECT_EQ(land_message::HelpTextOf(3), "HELP_TEXT_LAST_LINES_02");
	EXPECT_EQ(land_message::HelpTextOf(4), "HELP_TEXT_LAST_LINES_03");
}

TEST(LandMessage, EveryOtherNumberShowsTheLineForOneGod)
{
	for (const auto number : {1, 2, 5, 8, -1})
	{
		EXPECT_EQ(land_message::HelpTextOf(number), "HELP_TEXT_LAST_LINES_01") << number;
	}
}

TEST(LandInfoBox, TheTitleShowsTheBoxWithAnEmptyList)
{
	BoxFixture fixture;
	fixture.box.AddLine(u"old");
	fixture.Show(u"Two Gods");
	EXPECT_TRUE(fixture.box.IsActive());
	EXPECT_EQ(fixture.box.GetTitle().text, u"Two Gods");
	EXPECT_TRUE(fixture.Lines().empty());
	fixture.box.AddLine(u"Battle another god");
	fixture.box.AddLine(u"second");
	EXPECT_EQ(fixture.Lines(), (std::vector<std::u16string> {u"Battle another god", u"second"}));
}

TEST(LandInfoBox, GoClosesTheBox)
{
	BoxFixture fixture;
	fixture.Show(u"Two Gods");
	EXPECT_TRUE(fixture.Click(LandInfoBox::k_Go.Centre()));
	EXPECT_FALSE(fixture.box.IsActive());
	EXPECT_EQ(fixture.box.TakeChoice(), LandInfoBox::Choice::Go);
	EXPECT_EQ(fixture.box.TakeChoice(), LandInfoBox::Choice::None);
}

TEST(LandInfoBox, BackClosesTheBoxToLeaveTheLand)
{
	BoxFixture fixture;
	fixture.Show(u"Two Gods");
	EXPECT_TRUE(fixture.Click(k_Back));
	EXPECT_FALSE(fixture.box.IsActive());
	EXPECT_EQ(fixture.box.TakeChoice(), LandInfoBox::Choice::Back);
}

TEST(LandInfoBox, ANewLandClosesTheBoxWithNoChoice)
{
	BoxFixture fixture;
	fixture.Show(u"Two Gods");
	fixture.box.Close();
	EXPECT_FALSE(fixture.box.IsActive());
	// The game does not go on to the land or leave it, because of the close
	EXPECT_EQ(fixture.box.TakeChoice(), LandInfoBox::Choice::None);
}

TEST(LandInfoBox, EscapeClosesTheBoxOnlyWithNoModifierKey)
{
	BoxFixture fixture;
	fixture.Show(u"Two Gods");
	fixture.box.KeyDown(SDLK_ESCAPE, KMOD_CTRL);
	EXPECT_TRUE(fixture.box.IsActive());
	fixture.box.KeyDown(SDLK_ESCAPE, KMOD_NONE);
	EXPECT_FALSE(fixture.box.IsActive());
	EXPECT_EQ(fixture.box.TakeChoice(), LandInfoBox::Choice::Go);
}

TEST(LandInfoBox, ACoveredBoxStaysButTakesNoInput)
{
	BoxFixture fixture;
	fixture.Show(u"Two Gods");
	fixture.box.SetCovered(true);
	EXPECT_TRUE(fixture.box.IsActive());
	EXPECT_FALSE(fixture.box.TakesInput());
	// Escape and Go! go to the box that covers it
	fixture.box.KeyDown(SDLK_ESCAPE, KMOD_NONE);
	EXPECT_FALSE(fixture.Click(LandInfoBox::k_Go.Centre()));
	EXPECT_TRUE(fixture.box.IsActive());
	EXPECT_EQ(fixture.box.TakeChoice(), LandInfoBox::Choice::None);
	// When the other box goes, the box takes the input again
	fixture.box.SetCovered(false);
	EXPECT_TRUE(fixture.box.TakesInput());
	fixture.box.KeyDown(SDLK_ESCAPE, KMOD_NONE);
	EXPECT_FALSE(fixture.box.IsActive());
	EXPECT_EQ(fixture.box.TakeChoice(), LandInfoBox::Choice::Go);
}

TEST(LandInfoBox, OnlyAShownBoxCanBeCovered)
{
	BoxFixture fixture;
	fixture.box.SetCovered(true);
	EXPECT_FALSE(fixture.box.IsCovered());
	fixture.Show(u"Two Gods");
	fixture.box.SetCovered(true);
	// A close and a new show both take the cover away
	fixture.box.Close();
	EXPECT_FALSE(fixture.box.IsCovered());
	fixture.Show(u"Two Gods");
	EXPECT_TRUE(fixture.box.TakesInput());
}

TEST(LandInfoBox, InASkirmishLeaveSkirmishGameAsksFirstAndYesLeaves)
{
	const auto texts = Texts();
	const auto font = LoadFont();
	GameMenu menu {texts, font, u"aaa", MenuSettings {}};
	menu.SetSkirmish(true);
	menu.Open();
	menu.Update(1.0f);
	const auto controls = menu.GetNamedControls();
	const auto leave = std::ranges::find(controls, std::u16string(u"WWW"), &Dialog::NamedControl::name);
	ASSERT_NE(leave, controls.end());
	const auto middle = leave->rect.Centre();
	menu.MouseMove(middle);
	menu.MouseDown(middle);
	EXPECT_EQ(menu.MouseUp(middle), GameMenu::Action::None);
	ASSERT_TRUE(menu.IsAskingToQuit());
	EXPECT_EQ(menu.GetQuestionText(), u"WAW");
	menu.Update(1.0f);
	// The Yes arrow at the bottom left of the question
	const glm::ivec2 yes {220, 358};
	menu.MouseMove(yes);
	menu.MouseDown(yes);
	EXPECT_EQ(menu.MouseUp(yes), GameMenu::Action::LeaveSkirmish);
}

TEST(LandInfoBox, OutOfASkirmishTheButtonStartsASkirmishAtOnce)
{
	const auto texts = Texts();
	const auto font = LoadFont();
	GameMenu menu {texts, font, u"aaa", MenuSettings {}};
	menu.Open();
	menu.Update(1.0f);
	const auto controls = menu.GetNamedControls();
	const auto start = std::ranges::find(controls, std::u16string(u"BBB"), &Dialog::NamedControl::name);
	ASSERT_NE(start, controls.end());
	const auto middle = start->rect.Centre();
	menu.MouseMove(middle);
	menu.MouseDown(middle);
	EXPECT_EQ(menu.MouseUp(middle), GameMenu::Action::StartSkirmish);
	EXPECT_FALSE(menu.IsAskingToQuit());
}
