/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PointerSpeed.h"

#include <algorithm>

#include <glm/common.hpp>

namespace openblack::input::pointer_speed
{

float Scale(glm::ivec2 screenSize)
{
	if (screenSize.x <= 0 || screenSize.y <= 0)
	{
		return 1.0f;
	}
	const auto size = static_cast<glm::vec2>(screenSize);
	return std::min(size.y, size.x * 0.75f) / k_ReferenceHeight;
}

void ScaledPointer::Place(glm::ivec2 position)
{
	// The middle of the pixel, so that a movement either way counts the same before it reaches the next
	_position = static_cast<glm::vec2>(position) + 0.5f;
}

glm::ivec2 ScaledPointer::Move(glm::ivec2 movement, float scale, glm::ivec2 screenSize)
{
	const auto last = glm::max(static_cast<glm::vec2>(screenSize) - 0.5f, glm::vec2(0.5f));
	_position = glm::clamp(_position + static_cast<glm::vec2>(movement) * scale, glm::vec2(0.5f), last);
	return Position();
}

glm::ivec2 ScaledPointer::Position() const
{
	return glm::ivec2(glm::floor(_position));
}

} // namespace openblack::input::pointer_speed
