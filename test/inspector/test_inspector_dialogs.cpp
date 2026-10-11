/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// gui.press on the game's own dialogs: the start-of-game question's controls found by name and clicked through input

#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <Gui/SkipBox.h>
#include <Gui/TextDatabase.h>
#include <Inspector/DialogControls.h>
#include <Story/NewGameChoice.h>
#include <gtest/gtest.h>

#include "../gui/FakeGameFont.h"

using namespace openblack::gui;
using namespace openblack::gui::test;
using namespace openblack::inspector;
using openblack::new_game_choice::Choice;

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

/// The box shown, with the inspector's input reaching it as the game hands it the mouse, the screen being the dialog
/// space
struct InspectedSkipBox
{
	TextDatabase texts = SkipTexts();
	GameFont font = LoadFont();
	SkipBox box {texts, font, DialogPainter::k_MidTextSize};
	glm::ivec2 pointer {400, 300};
	std::optional<Choice> answer;

	InspectedSkipBox() { box.Show(); }

	void Send(std::span<const InputEvent> events)
	{
		for (const auto& event : events)
		{
			switch (event.kind)
			{
			case InputEvent::Kind::PointerTo:
				pointer = event.position;
				box.MouseMove(pointer);
				break;
			case InputEvent::Kind::ButtonDown:
				box.MouseDown(pointer);
				break;
			case InputEvent::Kind::ButtonUp:
				if (const auto result = box.MouseUp(pointer); result.answer.has_value())
				{
					answer = result.answer;
				}
				break;
			default:
				break;
			}
		}
	}

	/// What gui.press does for the skip box: finds the control by name and clicks its middle
	bool Press(const std::vector<ButtonPathStep>& path)
	{
		const auto controls = SkipBoxControls(box);
		const auto found = FindDialogControl(controls, path);
		if (!std::holds_alternative<glm::ivec2>(found))
		{
			return false;
		}
		Send(ClickAt(std::get<glm::ivec2>(found)));
		return true;
	}
};

/// The middle of the OK button
constexpr glm::ivec2 k_Ok = SkipBox::k_ButtonPosition + (SkipBox::k_ButtonSize / 2);

} // namespace

// The inspector knows the box's OK and its answers' radio buttons by name, the radio buttons by their answers and by
// the developers' start names
TEST(InspectorDialogs, TheSkipBoxsControlsHaveNames)
{
	const InspectedSkipBox f;
	const auto controls = SkipBoxControls(f.box);
	ASSERT_EQ(controls.size(), 5u);
	EXPECT_EQ(controls[0].name, "AW");
	EXPECT_EQ(controls[0].kind, "button");
	const std::vector<std::string> answers {"A", "B", "AB", "BAD"};
	const std::vector<std::string> starts {"normal", "creature", "story", "old"};
	for (size_t i = 0; i < answers.size(); ++i)
	{
		EXPECT_EQ(controls[i + 1].name, answers[i]) << i;
		EXPECT_EQ(controls[i + 1].kind, "radio") << i;
		EXPECT_EQ(controls[i + 1].middle, SkipBox::GetRadioRect(i).Centre()) << i;
		EXPECT_EQ(controls[i + 1].aliases, std::vector<std::string> {starts[i]}) << i;
	}
}

// gui.press on the skip box: a radio button by its answer or its start's name, then OK, answers with that choice
TEST(InspectorDialogs, GuiPressPicksASkipBoxAnswerAndPressesOk)
{
	InspectedSkipBox f;
	ASSERT_TRUE(f.Press({std::string("b")}));
	EXPECT_EQ(f.box.GetSelection(), Choice::SkipToCreatureSelect);
	ASSERT_TRUE(f.Press({std::string("Story")}));
	EXPECT_EQ(f.box.GetSelection(), Choice::SkipCreatureTutorial);
	EXPECT_FALSE(f.answer.has_value());
	// There is one "AB" among the controls (the question isn't one), so no second
	EXPECT_FALSE(f.Press({1, std::string("AB")}));
	EXPECT_FALSE(f.Press({std::string("nothing")}));
	EXPECT_FALSE(f.Press({}));
	ASSERT_TRUE(f.Press({std::string("aw")}));
	ASSERT_TRUE(f.answer.has_value());
	EXPECT_EQ(*f.answer, Choice::SkipCreatureTutorial);
	EXPECT_FALSE(f.box.IsActive());
}

// A click followed at once by a move away answers, the move coming after the letting go; a move between the press and
// the letting go drags the click off the button, as with a real mouse, which is why input must keep its order
TEST(InspectorDialogs, AClickFollowedAtOnceByAMoveStillAnswers)
{
	const auto click = ClickAt(k_Ok);
	std::vector<InputEvent> events(click.begin(), click.end());
	events.push_back({.kind = InputEvent::Kind::PointerTo, .position = {400, 300}});

	InspectedSkipBox inOrder;
	inOrder.Send(events);
	ASSERT_TRUE(inOrder.answer.has_value());
	EXPECT_EQ(*inOrder.answer, Choice::StartNormally);

	InspectedSkipBox overtaken;
	std::swap(events[2], events[3]);
	overtaken.Send(events);
	EXPECT_FALSE(overtaken.answer.has_value());
}

// The game's text is named in plain letters, its spaces and line breaks single spaces
TEST(InspectorDialogs, ControlNamesAreThePlainText)
{
	EXPECT_EQ(PlainText(u"  Skip the\ncreature  tutorial "), "Skip the creature tutorial");
	EXPECT_EQ(PlainText(u"Café"), "Caf?");
	EXPECT_EQ(PlainText(u""), "");
}

// A control labelled with spaces only has no name to be pressed by, and is left out; the others are pressed in their
// middle
TEST(InspectorDialogs, ControlsWithoutANameAreLeftOut)
{
	const std::vector<Dialog::NamedControl> named {
	    {.name = u"OK", .kind = "button", .rect = {.min = {10, 20}, .max = {30, 40}}},
	    {.name = u" ", .kind = "button", .rect = {.min = {0, 0}, .max = {8, 8}}},
	    {.name = u"Cancel", .kind = "button", .rect = {.min = {100, 20}, .max = {120, 40}}},
	};
	const auto controls = DialogControls(named);
	ASSERT_EQ(controls.size(), 2u);
	EXPECT_EQ(controls[0].middle, glm::ivec2(20, 30));
	EXPECT_EQ(controls[1].name, "Cancel");
	EXPECT_EQ(std::get<glm::ivec2>(FindDialogControl(controls, {std::string("cancel")})), glm::ivec2(110, 30));
}
