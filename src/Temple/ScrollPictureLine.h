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

#include <glm/vec3.hpp>

namespace openblack::temple::scroll_pictures
{

/// A line of the scroll that holds a picture, in the world
struct WorldPicture
{
	enum class Kind : uint8_t
	{
		Gesture,
		Control,
	};
	Kind kind {Kind::Gesture};
	/// The gesture, or the action of the control
	int number {0};
	/// The top of the line, across the scroll where the last line of words before it starts
	glm::vec3 lineTop {0.0f};
	/// The same point one line lower
	glm::vec3 lineBelow {0.0f};
	/// The top left and the bottom right corners of the scroll's texture
	glm::vec3 textureTopLeft {0.0f};
	glm::vec3 textureBottomRight {0.0f};
};

} // namespace openblack::temple::scroll_pictures
