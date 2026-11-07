/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>
#include <variant>

#include <gtest/gtest.h>

#include "Help/ClickCue.h"
#include "Input/KeyBindings.h"
#include "Input/MouseWheel.h"

using namespace openblack::help::click_cue;
using openblack::help::control_icons::MouseClick;
using openblack::input::BindableActionMap;
using openblack::input::KeyBindingTable;
using openblack::input::KeyChord;
using openblack::input::MouseInput;

namespace
{
std::u16string FakeKeyName(SDL_Scancode key)
{
	return key == SDL_SCANCODE_SPACE ? u"Space" : u"?";
}

/// The default bindings with the move action on other inputs
KeyBindingTable MoveOn(std::optional<KeyChord> key, MouseInput mouse)
{
	KeyBindingTable table = openblack::input::k_DefaultKeyBindings;
	for (auto& binding : table)
	{
		if (binding.action == BindableActionMap::MOVE)
		{
			binding.key = key;
			binding.mouse = mouse;
		}
	}
	return table;
}
} // namespace

TEST(ClickCue, TheButtonBlinksTwoTenthsOfEveryHalfSecond)
{
	EXPECT_TRUE(ButtonLit(0));
	EXPECT_TRUE(ButtonLit(199));
	EXPECT_FALSE(ButtonLit(200));
	EXPECT_FALSE(ButtonLit(499));
	EXPECT_TRUE(ButtonLit(500));
}

TEST(ClickCue, SitsAtTheRightOfTheBoxsBottomThird)
{
	// A box 90 rows tall: the cue a third of it, its word two thirds of that
	const auto cue = Layout(1280, 600, 689, 1.0f, 50.0f, false, 30.0f);
	ASSERT_TRUE(cue.has_value());
	EXPECT_EQ(cue->icon.min, glm::vec2(1246.0f, 660.0f));
	EXPECT_EQ(cue->icon.max, glm::vec2(1276.0f, 690.0f));
	EXPECT_EQ(cue->iconGlow.min, cue->icon.min);
	EXPECT_EQ(cue->iconGlow.max, cue->icon.max);
	EXPECT_EQ(cue->size, 30);
	EXPECT_FLOAT_EQ(cue->labelSize, 20.0f);
	EXPECT_EQ(cue->labelAt, glm::vec2(1194.0f, 665.0f));
	EXPECT_EQ(cue->labelGlow.min, glm::vec2(1194.0f, 665.0f));
	EXPECT_EQ(cue->labelGlow.max, glm::vec2(1244.0f, 685.0f));
	EXPECT_EQ(LabelSize(600, 689, true), 24);
	EXPECT_EQ(CueSize(600, 689), 30);
}

TEST(ClickCue, AKeyCapMovesTheWordLeftByItsWidth)
{
	// A key cap 55.5 pixels wide: the line is the word, the cap and the gap
	const auto cue = Layout(1280, 600, 689, 1.0f, 50.0f, false, 55.5f);
	ASSERT_TRUE(cue.has_value());
	EXPECT_EQ(cue->labelAt, glm::vec2(1168.0f, 665.0f));
	EXPECT_EQ(cue->icon.min, glm::vec2(1220.0f, 660.0f));
	EXPECT_EQ(cue->icon.max, glm::vec2(1275.0f, 690.0f));
}

TEST(ClickCue, TheLeftButtonIsTheRightOneMirrored)
{
	const auto lit = BlinkCellFor(MouseClick::LeftButton, 2, 0);
	EXPECT_EQ(lit.uvMin, glm::vec2(0.5f, 0.5f));
	EXPECT_EQ(lit.uvMax, glm::vec2(0.25f, 0.75f));
	const auto dark = BlinkCellFor(MouseClick::LeftButton, 2, 300);
	EXPECT_EQ(dark.uvMin, glm::vec2(0.25f, 0.5f));
	EXPECT_EQ(dark.uvMax, glm::vec2(0.0f, 0.75f));
}

TEST(ClickCue, OtherButtonsBlinkWithNoMirror)
{
	EXPECT_EQ(BlinkCellFor(MouseClick::RightButton, 2, 0).uvMin, glm::vec2(0.25f, 0.5f));
	EXPECT_EQ(BlinkCellFor(MouseClick::RightButton, 2, 0).uvMax, glm::vec2(0.5f, 0.75f));
	EXPECT_EQ(BlinkCellFor(MouseClick::RightButton, 2, 300).uvMin, glm::vec2(0.0f, 0.5f));
	EXPECT_EQ(BlinkCellFor(MouseClick::RightButton, 2, 300).uvMax, glm::vec2(0.25f, 0.75f));
	EXPECT_EQ(BlinkCellFor(MouseClick::MiddleButton, 0, 100).uvMin, glm::vec2(0.75f, 0.0f));
	EXPECT_EQ(BlinkCellFor(MouseClick::MiddleButton, 0, 300).uvMin, glm::vec2(0.0f, 0.0f));
	// The wheel turns frame by frame and does not blink
	EXPECT_EQ(BlinkCellFor(MouseClick::WheelUp, 2, 300).uvMin,
	          openblack::help::control_icons::CellFor(MouseClick::WheelUp, 2, 300).uvMin);
}

TEST(ClickCue, ShowsTheInputOfTheMoveAction)
{
	const openblack::help::control_icons::KeyNameFn keyName = FakeKeyName;
	const auto left = PictureFor(openblack::input::k_DefaultKeyBindings, keyName);
	ASSERT_TRUE(left.has_value());
	EXPECT_EQ(std::get<MouseClick>(*left), MouseClick::LeftButton);

	const auto right = PictureFor(MoveOn(std::nullopt, MouseInput::RightButton), keyName);
	ASSERT_TRUE(right.has_value());
	EXPECT_EQ(std::get<MouseClick>(*right), MouseClick::RightButton);

	// A key binding shows the key cap, a mouse binding wins over a key
	const auto key = PictureFor(MoveOn(KeyChord {SDL_SCANCODE_SPACE}, MouseInput::None), keyName);
	ASSERT_TRUE(key.has_value());
	EXPECT_EQ(std::get<std::u16string>(*key), u"Space");
	const auto both = PictureFor(MoveOn(KeyChord {SDL_SCANCODE_SPACE}, MouseInput::MiddleButton), keyName);
	ASSERT_TRUE(both.has_value());
	EXPECT_EQ(std::get<MouseClick>(*both), MouseClick::MiddleButton);

	// No input: no cue
	EXPECT_FALSE(PictureFor(MoveOn(std::nullopt, MouseInput::None), keyName).has_value());
}

TEST(ClickCue, FadesInWithItsGlows)
{
	// Too faint to draw at first
	EXPECT_FALSE(Layout(1280, 600, 689, 0.01f, 50.0f, false, 30.0f).has_value());
	const auto half = Layout(1280, 600, 689, 0.5f, 50.0f, false, 30.0f);
	ASSERT_TRUE(half.has_value());
	EXPECT_FLOAT_EQ(half->labelColour.a, 127.0f / 255.0f);
	EXPECT_FLOAT_EQ(half->iconColour.a, 127.0f / 255.0f);
	EXPECT_FLOAT_EQ(half->iconGlowColour.a, 64.0f / 255.0f);
	EXPECT_FLOAT_EQ(half->labelGlowColour.a, 21.0f / 255.0f);
	EXPECT_EQ(half->labelColour, glm::vec4(1.0f, 1.0f, 0.0f, 127.0f / 255.0f));
	EXPECT_EQ(half->shadowColour, glm::vec4(0.0f, 0.0f, 0.0f, 127.0f / 255.0f));
}

TEST(ClickCue, TheMouseIsInTheRowOfThePlayersMouse)
{
	using openblack::platform_mouse::MousePictureRow;
	// A wheel mouse, a mouse with more than two buttons, and a two-button mouse
	EXPECT_EQ(MousePictureRow(true, 2), 2);
	EXPECT_EQ(MousePictureRow(true, 0), 2);
	EXPECT_EQ(MousePictureRow(false, 3), 1);
	EXPECT_EQ(MousePictureRow(false, 5), 1);
	EXPECT_EQ(MousePictureRow(false, 2), 0);
	EXPECT_EQ(MousePictureRow(false, 0), 0);

	const auto twoButtons = BlinkCellFor(MouseClick::LeftButton, MousePictureRow(false, 2), 300);
	EXPECT_EQ(twoButtons.uvMin, glm::vec2(0.25f, 0.0f));
	EXPECT_EQ(twoButtons.uvMax, glm::vec2(0.0f, 0.25f));
	const auto threeButtons = BlinkCellFor(MouseClick::LeftButton, MousePictureRow(false, 3), 300);
	EXPECT_EQ(threeButtons.uvMin, glm::vec2(0.25f, 0.25f));
	EXPECT_EQ(threeButtons.uvMax, glm::vec2(0.0f, 0.5f));
}
