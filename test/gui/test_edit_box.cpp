/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>

#include <SDL_keycode.h>
#include <gtest/gtest.h>

#include "FakeGameFont.h"
#include "Gui/Controls.h"

using openblack::gui::DialogRect;
using openblack::gui::EditBox;
using openblack::gui::GameFont;

namespace
{
constexpr DialogRect k_Rect {.min = {150, 525}, .max = {650, 555}};
} // namespace

TEST(EditBox, ABoxThatThePlayerCanTypeIntoTakesCharactersAndKeys)
{
	const GameFont font = openblack::gui::test::LoadFont();
	EditBox box(font, k_Rect, u"ab", 20, 10);
	box.SetFocus(true);
	EXPECT_TRUE(box.KeyDown(SDLK_END));
	EXPECT_TRUE(box.TextInput(u"c"));
	EXPECT_EQ(box.GetText(), u"abc");
	EXPECT_TRUE(box.KeyDown(SDLK_BACKSPACE));
	EXPECT_EQ(box.GetText(), u"ab");
	EXPECT_TRUE(box.PassesFocusOnReturn());
}

TEST(EditBox, ABoxThatThePlayerCannotTypeIntoTakesNoCharacterOrKey)
{
	const GameFont font = openblack::gui::test::LoadFont();
	EditBox box(font, k_Rect, u"Status", 20, 10);
	box.editable = false;
	box.SetFocus(true);
	EXPECT_FALSE(box.TextInput(u"x"));
	EXPECT_FALSE(box.KeyDown(SDLK_BACKSPACE));
	EXPECT_FALSE(box.KeyDown(SDLK_DELETE));
	EXPECT_FALSE(box.KeyDown(SDLK_HOME));
	EXPECT_EQ(box.GetText(), u"Status");
	// Return does nothing in the box, so it does not pass the focus on
	EXPECT_FALSE(box.PassesFocusOnReturn());
}

TEST(EditBox, TheFrameOfABoxThatThePlayerCannotTypeIntoShowsAtHalfOpacityInWholeSteps)
{
	static_assert(EditBox::FrameOpacityWhenNotEditable(255) == 127);
	static_assert(EditBox::FrameOpacityWhenNotEditable(128) == 64);
	static_assert(EditBox::FrameOpacityWhenNotEditable(1) == 0);
	// The opacity doubles again after the frame, so an odd opacity loses one step
	static_assert(EditBox::OpacityAfterFrameWhenNotEditable(255) == 254);
	static_assert(EditBox::OpacityAfterFrameWhenNotEditable(128) == 128);
	static_assert(EditBox::OpacityAfterFrameWhenNotEditable(0) == 0);
	SUCCEED();
}
