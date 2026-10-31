/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <string_view>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "DialogPainter.h"
#include "SavingBoxRules.h"

namespace openblack::gui::saving_box
{

/// A part of the box with four corners in the dialog space, and the colours of the corners: top left, top right, bottom
/// right and bottom left
struct Quad
{
	std::array<glm::vec2, 4> corners;
	std::array<uint32_t, 4> colours;
};

/// Where the parts of the box go, in the dialog space
struct Layout
{
	/// The darker screen (when the box makes the screen darker), the two soft shadows, then the three parts of the band
	std::array<Quad, 6> quads;
	/// The lines along the top and the bottom of the band go from lineFrom to lineTo
	int lineFrom {0};
	int lineTo {0};
	int top {0};
	int bottom {0};
};

/// The layout for a screen whose left and right edges are at these x of the dialog space
[[nodiscard]] Layout LayoutFor(int left, int right);

/// The top of the text in its rectangle: centred up and down when the text is not as high as the rectangle
[[nodiscard]] int TextTop(const DialogRect& rect, float textHeight);

/// Draws the box over everything
void Draw(const DialogPainter& painter, glm::u16vec2 resolution, std::u16string_view text, bool darken);

} // namespace openblack::gui::saving_box
