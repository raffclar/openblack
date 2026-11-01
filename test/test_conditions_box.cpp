/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <initializer_list>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Gui/ConditionsBoxRules.h"
#include "PlayerSlots.h"

using namespace openblack;
using namespace openblack::gui::conditions_box;
using openblack::ecs::components::WinCondition;

// The names of the conditions are the multiplayer texts, which skip the number 13
TEST(ConditionsBox, TheConditionsTakeTheMultiplayerTexts)
{
	EXPECT_EQ(ConditionTextName(0), "HELP_TEXT_MULTIPLAYER_01");
	EXPECT_EQ(ConditionTextName(11), "HELP_TEXT_MULTIPLAYER_12");
	EXPECT_EQ(ConditionTextName(12), "HELP_TEXT_MULTIPLAYER_14");
	EXPECT_EQ(ConditionTextName(14), "HELP_TEXT_MULTIPLAYER_16");
	EXPECT_TRUE(ConditionTextName(15).empty());
}

// A line of the list: the name of the condition and its target in brackets
TEST(ConditionsBox, ALineShowsTheNameAndTheTarget)
{
	EXPECT_EQ(LineText(u"Number of trees grown", 250), u"Number of trees grown (250)");
}

// How far a god got: its value, never more than the target, or Done
TEST(ConditionsBox, AValueIsCutToTheTargetAndACompletedConditionIsDone)
{
	EXPECT_EQ(ValueText({.current = 12, .target = 50, .completed = false}, u"Done"), u"12");
	EXPECT_EQ(ValueText({.current = 80, .target = 50, .completed = false}, u"Done"), u"50");
	EXPECT_EQ(ValueText({.current = 3, .target = 50, .completed = true}, u"Done"), u"Done");
}

// The values take the right third of the line, one column for each god shown
TEST(ConditionsBox, TheValuesTakeTheRightThirdOfTheLine)
{
	// A list from 40 to 731 (the box right of the scroll bar): a third is 230
	const auto three = ValueColumns(40, 731, 3);
	EXPECT_EQ(three.first, 500);
	EXPECT_EQ(three.width, 76);
	const auto one = ValueColumns(40, 731, 1);
	EXPECT_EQ(one.first, 500);
	EXPECT_EQ(one.width, 230);
	EXPECT_EQ(ValueColumns(40, 731, 0).width, 0);
}

namespace
{
Figures GodsOf(std::initializer_list<PlayerNames> players)
{
	Figures figures;
	for (const auto player : players)
	{
		figures.gods.push_back({.player = player, .colour = {}, .scores = {}, .conditions = {}});
	}
	return figures;
}
} // namespace

// The box shows the gods whose check box is ticked, in the order of the players
TEST(ConditionsBox, TheCheckBoxesPickTheGods)
{
	const auto figures = GodsOf({PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_THREE});
	const auto boxes = GodsOfCheckBoxes(figures, 0);
	// Each check box has the god of its player
	EXPECT_EQ(boxes.at(0), std::optional<size_t> {0});
	EXPECT_FALSE(boxes.at(1).has_value());
	EXPECT_EQ(boxes.at(2), std::optional<size_t> {1});
	EXPECT_FALSE(boxes.at(3).has_value());
	EXPECT_EQ(ShownGods(boxes, {true, true}), (std::vector<size_t> {0, 1}));
	EXPECT_EQ(ShownGods(boxes, {true, false}), (std::vector<size_t> {0}));
	EXPECT_TRUE(ShownGods(boxes, {false, false}).empty());
	// With 4 gods or fewer the labels are the places of the check boxes, as in the game
	EXPECT_EQ(CheckBoxLabel(figures, boxes, 0), u"Player 1");
	EXPECT_EQ(CheckBoxLabel(figures, boxes, 3), u"Player 4");
}

// With more than 4 gods the check boxes have the gods of the window, labelled with the numbers of the gods
TEST(ConditionsBox, WithMoreThanFourGodsTheCheckBoxesFollowTheWindow)
{
	Figures figures;
	for (size_t slot = 0; slot < 10; ++slot)
	{
		figures.gods.push_back(
		    {.player = player_slots::PlayerOfSlot(slot).value(), .colour = {}, .scores = {}, .conditions = {}});
	}
	const auto first = GodsOfCheckBoxes(figures, 0);
	EXPECT_EQ(first.at(0), std::optional<size_t> {0});
	EXPECT_EQ(first.at(3), std::optional<size_t> {3});
	// The last page shows the last 4 gods
	const auto last = GodsOfCheckBoxes(figures, 8);
	EXPECT_EQ(last.at(0), std::optional<size_t> {6});
	EXPECT_EQ(last.at(3), std::optional<size_t> {9});
	// The god of slot 8 has the player number after the neutral player, and its label gives its slot
	EXPECT_EQ(CheckBoxLabel(figures, last, 0), u"Player 7");
	EXPECT_EQ(CheckBoxLabel(figures, last, 1), u"Player 8");
	EXPECT_EQ(CheckBoxLabel(figures, last, 3), u"Player 10");
	// A god of another page that is not ticked does not change the gods shown
	std::vector<bool> ticked(10, true);
	ticked.at(0) = false;
	ticked.at(7) = false;
	EXPECT_EQ(ShownGods(last, ticked), (std::vector<size_t> {6, 8, 9}));
}
