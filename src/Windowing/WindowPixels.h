/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec2.hpp>

/// The window units of the mouse events against the pixels of the back buffer.
///
/// In a web browser the window units are the CSS pixels of the page. The game draws at the CSS size times the device
/// pixel ratio, thus a screen with a ratio of 2 has two pixels of the back buffer for each CSS pixel. The game reads
/// the pointer in the pixels of the back buffer, as the desktop game does. On the desktop the two units are the same.
namespace openblack::windowing
{

/// The pixels of the back buffer for one window unit, on each axis. A window of no size has a scale of 1.
[[nodiscard]] glm::vec2 PixelScale(glm::ivec2 windowSize, glm::ivec2 pixelSize);

/// The pixel of the back buffer at the top left of a window unit. At a scale of 1 the place does not change.
[[nodiscard]] glm::ivec2 PointToPixels(glm::ivec2 point, glm::vec2 scale);

/// The movements of the mouse in pixels of the back buffer. The parts of a pixel that a scale such as 1.5 leaves are
/// kept for the next movement, so that slow movements add up.
class MovementToPixels
{
public:
	/// The movement in whole pixels. The part of a pixel that stays goes to the next movement.
	[[nodiscard]] glm::ivec2 Move(glm::ivec2 movement, glm::vec2 scale);

private:
	glm::vec2 _remainder {0.0f, 0.0f};
};

} // namespace openblack::windowing
