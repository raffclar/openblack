/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "3D/Extent.h"

/// The values that every draw of a frame reads with the same value. They go to the GPU one time for each frame.
namespace openblack::graphics
{

/// The land's extent as the shaders read it: the lowest corner, then the highest corner. All zero when the game has
/// no land.
[[nodiscard]] inline glm::vec4 IslandExtentValue(const std::optional<Extent2>& extent)
{
	return extent.has_value() ? glm::vec4(extent->minimum, extent->maximum) : glm::vec4(0.0f);
}

} // namespace openblack::graphics
