/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Gui/Controls.h"

using openblack::gui::ColourPicker;
using openblack::gui::DialogRect;

namespace
{
ColourPicker MakePicker(ColourPicker::Kind kind)
{
	return {DialogRect {.min = {10, 100}, .max = {30, 300}}, kind};
}
} // namespace

TEST(ColourPicker, TheArrowStartsHalfWayDownAndTheBarIsSeeThroughGrey)
{
	const auto picker = MakePicker(ColourPicker::Kind::Brightness);
	EXPECT_FLOAT_EQ(picker.GetPosition(), 0.5f);
	EXPECT_EQ(picker.colour.r, 128);
	EXPECT_EQ(picker.colour.a, 0);
}

TEST(ColourPicker, ADragMovesTheArrowToThePointerInsideTheBar)
{
	auto picker = MakePicker(ColourPicker::Kind::Palette);
	std::optional<glm::ivec2> dragged;
	picker.onDrag = [&dragged](glm::ivec2 point) { dragged = point; };
	picker.Drag({20, 150});
	EXPECT_FLOAT_EQ(picker.GetPosition(), 0.25f);
	ASSERT_TRUE(dragged.has_value());
	EXPECT_EQ(dragged->y, 150);
	// Above the top and below the bottom, the arrow stays at the ends
	picker.Drag({20, 40});
	EXPECT_FLOAT_EQ(picker.GetPosition(), 0.0f);
	picker.Drag({20, 900});
	EXPECT_FLOAT_EQ(picker.GetPosition(), 1.0f);
}
