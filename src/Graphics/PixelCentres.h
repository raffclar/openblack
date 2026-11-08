/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

/// The pixel centres of the game's screen. The game puts the centre of a pixel at whole coordinates. A modern renderer
/// puts the centre half a pixel further right and down. These rules let the modern renderer fill the same pixels as the
/// game, for the 3D views and for the interface.
namespace openblack::graphics::pixel_centres
{

/// The distance from the game's centre of a pixel to the centre of the same pixel in a modern renderer, right and down
constexpr float k_GameToModern = 0.5f;

/// The projection that puts a point on the same pixel of a target as the game's projection does. The game's projection
/// puts the edge of its view on the centre of the first pixel, with no offset. Thus each point moves half a pixel right
/// and down, and the 3D world shows on the same pixels as in the game. `target` is the size of the view in pixels.
[[nodiscard]] constexpr glm::mat4 OnTarget(const glm::mat4& projection, glm::vec2 target) noexcept
{
	// A view with no pixels has no pixel centres to move to
	if (target.x <= 0.0f || target.y <= 0.0f)
	{
		return projection;
	}
	// One pixel is 2 / size of the clip space across the view. Up in clip space is up on the screen.
	const glm::vec2 shift {2.0f * k_GameToModern / target.x, -2.0f * k_GameToModern / target.y};
	auto moved = projection;
	for (glm::length_t column = 0; column < 4; ++column)
	{
		moved[column][0] += shift.x * projection[column][3];
		moved[column][1] += shift.y * projection[column][3];
	}
	return moved;
}

} // namespace openblack::graphics::pixel_centres
