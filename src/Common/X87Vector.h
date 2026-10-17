/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>

#include <glm/vec3.hpp>

/// The game's sums over the three parts of a vector, in the game's order.
///
/// A sum of three floats can change in its last bit when the order changes. `glm::dot` and `glm::length` add x, then y,
/// then z. Use these functions where the game adds z first, then y, then x.
namespace openblack::x87
{

/// The dot product, added from z to x
[[nodiscard]] constexpr float DotZyx(const glm::vec3& a, const glm::vec3& b)
{
	return ((a.z * b.z) + (a.y * b.y)) + (a.x * b.x);
}

/// The length, with the squares added from z to x
[[nodiscard]] inline float LengthZyx(const glm::vec3& v)
{
	return std::sqrt(DotZyx(v, v));
}

} // namespace openblack::x87
