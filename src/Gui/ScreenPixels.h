/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <optional>

#include <glm/vec2.hpp>

#include "Graphics/PixelCentres.h"

/// The pixel rules of the game's screen draws. The game puts the centre of a pixel at whole coordinates, and a modern
/// renderer puts it half a pixel further right and down. Thus a screen quad of the game reads its texture half a pixel
/// further on than the same quad in a modern renderer.
namespace openblack::gui::screen_pixels
{

/// How a screen draw of the game places the corners that it gets
enum class Corners : uint8_t
{
	/// The corners stay where they are, as the game's text, mouse pictures, gesture pictures and filled rectangles do
	AsGiven,
	/// The corners move half a pixel right and down, as the game's box draw does: the boxes, buttons, key caps, glows,
	/// arrows and the pointer
	BoxDraw,
};

/// The distance from the game's centre of a pixel to the centre of the same pixel in a modern renderer. The 3D views
/// use the same distance.
constexpr float k_PixelCentre = graphics::pixel_centres::k_GameToModern;
/// The distance that the game's box draw moves each corner, right and down
constexpr float k_BoxDrawShift = 0.5f;

/// The distance that a draw moves the corners that it gets, on the game's screen
[[nodiscard]] constexpr float CornerShift(Corners corners) noexcept
{
	return corners == Corners::BoxDraw ? k_BoxDrawShift : 0.0f;
}

/// The point in the render target of a corner on the game's screen. The game and the target then fill the same pixels,
/// and each pixel reads the same point of the texture.
[[nodiscard]] constexpr glm::vec2 ToTarget(glm::vec2 corner, Corners corners) noexcept
{
	return corner + (CornerShift(corners) + k_PixelCentre);
}

/// The first pixel inside a clip edge on the game's screen. The game shows a pixel when its centre is on or after the
/// edge, and the edge moves with the corners of the draw. The pixel is in 0 to most.
[[nodiscard]] inline uint16_t ClipEdge(float edge, Corners corners, uint16_t most) noexcept
{
	const auto first = std::ceil(edge + CornerShift(corners));
	return static_cast<uint16_t>(std::clamp(first, 0.0f, static_cast<float>(most)));
}

/// The pixels that a line fills, on the game's screen, from the first to the last in each direction
struct LineSpan
{
	glm::ivec2 first;
	glm::ivec2 last;
};

/// The pixels that the game's line draw fills for a line along a row or a column, between the pixels `from` and `to`.
/// The game adds half a pixel to the two ends, thus the line runs on the boundary between two rows (or two columns).
/// The graphics card fills a pixel when the line leaves the diamond round the centre of the pixel. The diamond keeps its
/// bottom corner, and also its right corner for a line along a column. Thus:
/// - a line along a row fills that row, from the pixel after the left end to the right end;
/// - a line along a column fills that column, from the pixel under the top end to the bottom end.
/// The end of the line is in no diamond, thus the last pixel rule of the game adds no pixel. A line of no length fills
/// no pixel. The game's setup draws use no other lines.
[[nodiscard]] constexpr std::optional<LineSpan> LinePixels(glm::ivec2 from, glm::ivec2 to) noexcept
{
	if (from.y == to.y && from.x != to.x)
	{
		return LineSpan {.first = {std::min(from.x, to.x) + 1, from.y}, .last = {std::max(from.x, to.x), from.y}};
	}
	if (from.x == to.x && from.y != to.y)
	{
		return LineSpan {.first = {from.x, std::min(from.y, to.y) + 1}, .last = {from.x, std::max(from.y, to.y)}};
	}
	return std::nullopt;
}

} // namespace openblack::gui::screen_pixels
