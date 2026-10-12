/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace openblack::temple_lanterns
{

/// The two lanterns at a temple's entrance appear once more than this much of the temple is built, of 1
inline constexpr float k_BuiltAbove = 0.07f;

/// Where each lantern stands in the temple's own space, on the temple's ground: the point the game's heart gives for each
inline constexpr std::array<glm::vec3, 2> k_Positions = {
    glm::vec3(-8.85f, 0.0f, -10.34f),
    glm::vec3(-2.3f, 0.0f, -13.4f),
};

/// Whether the lanterns are there, by how much of the temple is built
[[nodiscard]] inline bool Present(float built)
{
	return built > k_BuiltAbove;
}

/// Where a lantern stands in the world, placed with the temple like the temple's own model is: scaled, turned, then moved
[[nodiscard]] inline glm::vec3 WorldPosition(size_t lantern, const glm::vec3& templePosition, const glm::mat3& templeRotation,
                                             const glm::vec3& templeScale)
{
	return templePosition + templeRotation * (templeScale * k_Positions.at(lantern));
}

} // namespace openblack::temple_lanterns
