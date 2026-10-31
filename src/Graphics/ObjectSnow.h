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

#include <algorithm>

#include <glm/vec3.hpp>

/// Where the snow covers an object, and what colour the object gets there. The game draws each object with snow a second
/// time with the snow texture. A texel shows the snow where the alpha of the snow texture is at least 255 less the snow
/// level of the object. There the snow replaces the colour of the object, with no blend. The same rule is true for
/// buildings, trees, features, rocks, villagers, animals and the faces of broken shapes. assets/shaders/snow_object.sh is
/// the shader side.
namespace openblack::object_snow
{

/// The highest snow level, and the highest alpha of the snow texture
constexpr int32_t k_MaxLevel = 255;

/// The lowest alpha of the snow texture that shows the snow at `level`, 0 to 255
[[nodiscard]] constexpr int32_t Threshold(int32_t level)
{
	return k_MaxLevel - std::clamp(level, 0, k_MaxLevel);
}

/// Whether a texel with the snow texture alpha `alpha`, 0 to 255, shows the snow at `level`. A level of 0 shows no snow.
[[nodiscard]] constexpr bool Shows(int32_t level, int32_t alpha)
{
	return level > 0 && alpha >= Threshold(level);
}

/// The colour of a texel of an object: the snow colour where the snow shows, else the colour of the object
[[nodiscard]] constexpr glm::vec3 Covered(const glm::vec3& object, const glm::vec3& snow, int32_t level, int32_t alpha)
{
	return Shows(level, alpha) ? snow : object;
}

} // namespace openblack::object_snow
