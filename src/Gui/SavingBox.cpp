/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SavingBox.h"

#include <SaveSlots.h>

using namespace openblack::gui;

namespace
{
glm::vec4 ColourOf(uint32_t argb)
{
	return {static_cast<float>((argb >> 16) & 0xFF) / 255.0f, static_cast<float>((argb >> 8) & 0xFF) / 255.0f,
	        static_cast<float>(argb & 0xFF) / 255.0f, static_cast<float>((argb >> 24) & 0xFF) / 255.0f};
}

/// A box from one corner to the other. When the second y is above the first, the box turns over, with its colours.
saving_box::Quad Box(int x1, int y1, int x2, int y2, std::array<uint32_t, 4> colours)
{
	const auto fx1 = static_cast<float>(x1);
	const auto fy1 = static_cast<float>(y1);
	const auto fx2 = static_cast<float>(x2);
	const auto fy2 = static_cast<float>(y2);
	return {.corners = {glm::vec2 {fx1, fy1}, glm::vec2 {fx2, fy1}, glm::vec2 {fx2, fy2}, glm::vec2 {fx1, fy2}},
	        .colours = colours};
}
} // namespace

bool saving_box::DarkensScreen(uint32_t slot)
{
	return slot < gamesave::k_FirstAutosaveSlot || slot > gamesave::k_LastAutosaveSlot;
}

saving_box::Layout saving_box::LayoutFor(int left, int right)
{
	const int middle = DialogPainter::k_Size.y / 2;
	const int top = middle - k_HalfBand;
	const int bottom = middle + k_HalfBand;
	// The ends shade over a part of the width, as a whole number of units
	const int width = right - left;
	const int end = left + static_cast<int>(static_cast<float>(width) * k_EndShare);
	const int otherEnd = right - (end - left);
	return {
	    .quads = {Box(left - 1, -1, right + 1, DialogPainter::k_Size.y + 1,
	                  {k_DarkColour, k_DarkColour, k_DarkColour, k_DarkColour}),
	              // The soft shadows: clear away from the band, darkest at its edge
	              Box(left - 1, top - k_Shadow, right, top, {0, 0, k_ShadowColour, k_ShadowColour}),
	              Box(left - 1, bottom, right, bottom + k_Shadow, {k_ShadowColour, k_ShadowColour, 0, 0}),
	              // The band: darker at each end of the screen
	              Box(left - 1, top, end, bottom, {k_BandDark, k_BandLight, k_BandLight, k_BandDark}),
	              Box(end, top, otherEnd, bottom, {k_BandLight, k_BandLight, k_BandLight, k_BandLight}),
	              Box(otherEnd, top, right, bottom, {k_BandLight, k_BandDark, k_BandDark, k_BandLight})},
	    .lineFrom = left - 1,
	    .lineTo = right,
	    .top = top,
	    .bottom = bottom,
	};
}

int saving_box::TextTop(const DialogRect& rect, float textHeight)
{
	const auto height = static_cast<float>(rect.Height());
	if (!(textHeight < height))
	{
		return rect.min.y;
	}
	return static_cast<int>(((height - textHeight) * 0.5f) + static_cast<float>(rect.min.y));
}

void saving_box::Draw(const DialogPainter& painter, glm::u16vec2 resolution, std::u16string_view text, bool darken)
{
	const auto left = painter.EdgeToDialog({0, 0}).x;
	const auto right = painter.EdgeToDialog({resolution.x, resolution.y}).x;
	const auto layout = LayoutFor(left, right);
	for (size_t i = darken ? 0 : 1; i < layout.quads.size(); ++i)
	{
		const auto& quad = layout.quads.at(i);
		painter.DrawShape(quad.corners, {ColourOf(quad.colours[0]), ColourOf(quad.colours[1]), ColourOf(quad.colours[2]),
		                                 ColourOf(quad.colours[3])});
	}
	painter.DrawLine({layout.lineFrom, layout.top}, {layout.lineTo, layout.top}, ColourOf(k_LineColour));
	painter.DrawLine({layout.lineFrom, layout.bottom}, {layout.lineTo, layout.bottom}, ColourOf(k_LineColour));
	// The shadow first, a unit right and down, then the text
	const auto height = painter.GetTextHeight(k_TextRect.Width(), text, k_TextSize);
	const auto top = TextTop(k_TextRect, height);
	const DialogRect shadow {.min = {k_TextRect.min.x + 1, top + 1}, .max = {k_TextRect.max.x + 1, k_TextRect.max.y + 1}};
	const DialogRect words {.min = {k_TextRect.min.x, top}, .max = k_TextRect.max};
	painter.DrawTextWrapped(shadow, true, text, k_TextSize, ColourOf(k_TextShadowColour));
	painter.DrawTextWrapped(words, true, text, k_TextSize, ColourOf(k_TextColour));
}
