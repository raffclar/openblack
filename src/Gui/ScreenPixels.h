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

#include <glm/vec2.hpp>

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

/// The distance from the game's centre of a pixel to the centre of the same pixel in a modern renderer
constexpr float k_PixelCentre = 0.5f;
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

} // namespace openblack::gui::screen_pixels
