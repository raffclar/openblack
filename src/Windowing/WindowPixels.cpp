/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WindowPixels.h"

#include <glm/common.hpp>

namespace openblack::windowing
{

glm::vec2 PixelScale(glm::ivec2 windowSize, glm::ivec2 pixelSize)
{
	if (windowSize.x <= 0 || windowSize.y <= 0 || pixelSize.x <= 0 || pixelSize.y <= 0)
	{
		return glm::vec2(1.0f);
	}
	return static_cast<glm::vec2>(pixelSize) / static_cast<glm::vec2>(windowSize);
}

glm::ivec2 PointToPixels(glm::ivec2 point, glm::vec2 scale)
{
	return glm::ivec2(glm::floor(static_cast<glm::vec2>(point) * scale));
}

glm::ivec2 MovementToPixels::Move(glm::ivec2 movement, glm::vec2 scale)
{
	const auto exact = static_cast<glm::vec2>(movement) * scale + _remainder;
	// The whole pixels go toward zero, so that the part that stays has the sign of the movement
	const auto whole = glm::trunc(exact);
	_remainder = exact - whole;
	return glm::ivec2(whole);
}

} // namespace openblack::windowing
