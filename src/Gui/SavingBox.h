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

/// The box that the game shows while it saves or loads: "Saving..." or "Loading..." on a grey band across the middle
/// of the screen. The game draws the box as one frame of its own over the last picture and shows that frame. Then it
/// saves or loads while the frame stays on the screen. The screen also gets darker, but not for an automatic save.
namespace openblack::gui::saving_box
{

/// The help texts of the box
inline constexpr std::string_view k_SavingText = "HELP_TEXT_DIALOG_ADDITION_120";
inline constexpr std::string_view k_LoadingText = "HELP_TEXT_DIALOG_ADDITION_85";
/// The big text size of the game
inline constexpr int k_TextSize = 35;
/// The band goes this far up and down from the middle of the screen, and its soft shadows are this deep
inline constexpr int k_HalfBand = 30;
inline constexpr int k_Shadow = 15;
/// Each end of the band shades over this part of the width of the screen
inline constexpr float k_EndShare = 0.4f;
/// The colours, as 0xAARRGGBB
inline constexpr uint32_t k_DarkColour = 0x78000000;
inline constexpr uint32_t k_ShadowColour = 0x9F000000;
inline constexpr uint32_t k_BandDark = 0xFF202020;
inline constexpr uint32_t k_BandLight = 0xFF404040;
inline constexpr uint32_t k_LineColour = 0xF0F0F0F0;
inline constexpr uint32_t k_TextShadowColour = 0xFF000000;
inline constexpr uint32_t k_TextColour = 0xFFFFFFFF;
/// The text is centred across and up and down in this rectangle of the dialog space. Its shadow is a unit right and a
/// unit down.
inline constexpr DialogRect k_TextRect {.min = {100, 200}, .max = {700, 400}};

/// Whether the box makes the screen darker: not for the slots of the automatic saves
[[nodiscard]] bool DarkensScreen(uint32_t slot);

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
