/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Gui/ConditionsBoxRules.h"

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

// The box shows the gods whose check box is ticked, in the order of the players
TEST(ConditionsBox, TheCheckBoxesPickTheGods)
{
	Figures figures;
	figures.gods.push_back({.player = PlayerNames::PLAYER_ONE, .colour = {}, .scores = {}, .conditions = {}});
	figures.gods.push_back({.player = PlayerNames::PLAYER_THREE, .colour = {}, .scores = {}, .conditions = {}});
	EXPECT_EQ(ShownGods(figures, {true, true, true, true}), (std::vector<size_t> {0, 1}));
	EXPECT_EQ(ShownGods(figures, {true, true, false, true}), (std::vector<size_t> {0}));
	EXPECT_TRUE(ShownGods(figures, {false, false, false, false}).empty());
}
