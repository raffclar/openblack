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

namespace openblack::ecs::components
{

struct Fixed
{
	/// An empty value, which the load of a saved world fills
	Fixed() = default;
	Fixed(const glm::vec2& boundingCenter, float boundingRadius)
	    : boundingCenter(boundingCenter)
	    , boundingRadius(boundingRadius)
	{
	}

	glm::vec2 boundingCenter {0.0f};
	float boundingRadius {0.0f};
};

} // namespace openblack::ecs::components
