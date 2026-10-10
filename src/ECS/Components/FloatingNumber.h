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

#include <string>

#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// A number floating up from a point, as the share a totem was left at: it rises and, in its last second, fades
struct FloatingNumber
{
	std::string text;
	glm::vec3 position {0.0f};
	/// Its colour, 0xAARRGGBB; its alpha follows its life
	uint32_t colour {0xFFFFFFFF};
	/// Seconds left
	float life {5.0f};
};

} // namespace openblack::ecs::components
