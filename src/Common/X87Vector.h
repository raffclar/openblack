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
#include <cstdint>

#include <glm/vec3.hpp>

/// The game's sums over the three parts of a vector, in the game's order.
///
/// A sum of three floats can change in its last bit when the order changes. `glm::dot`, `glm::length`,
/// `glm::distance` and `glm::normalize` add x, then y, then z. The game adds the parts in different orders in different
/// places. Read the order from the game's instructions, and use these functions with that order.
namespace openblack::x87
{

/// The order in which the game adds the three parts. `Xzy` adds x and z first, then adds y to that sum.
enum class SumOrder : uint8_t
{
	Xyz,
	Xzy,
	Yxz,
	Yzx,
	Zxy,
	Zyx,
};

/// The sum of three parts, in the given order
template <SumOrder Order>
[[nodiscard]] constexpr float Sum(float x, float y, float z)
{
	if constexpr (Order == SumOrder::Xyz)
	{
		return (x + y) + z;
	}
	else if constexpr (Order == SumOrder::Xzy)
	{
		return (x + z) + y;
	}
	else if constexpr (Order == SumOrder::Yxz)
	{
		return (y + x) + z;
	}
	else if constexpr (Order == SumOrder::Yzx)
	{
		return (y + z) + x;
	}
	else if constexpr (Order == SumOrder::Zxy)
	{
		return (z + x) + y;
	}
	else
	{
		return (z + y) + x;
	}
}

/// The dot product, with the products added in the given order
template <SumOrder Order>
[[nodiscard]] constexpr float Dot(const glm::vec3& a, const glm::vec3& b)
{
	return Sum<Order>(a.x * b.x, a.y * b.y, a.z * b.z);
}

/// The length, with the squares added in the given order
template <SumOrder Order>
[[nodiscard]] inline float Length(const glm::vec3& v)
{
	return std::sqrt(Dot<Order>(v, v));
}

/// The distance between two points, with the squares added in the given order
template <SumOrder Order>
[[nodiscard]] inline float Distance(const glm::vec3& a, const glm::vec3& b)
{
	return Length<Order>(a - b);
}

/// The vector times `scale / length`. The game divides the scale by the length one time, then multiplies each part
/// by that share. With a scale of 1, this gives the unit vector.
template <SumOrder Order>
[[nodiscard]] inline glm::vec3 Scaled(const glm::vec3& v, float scale = 1.0f)
{
	const float share = scale / Length<Order>(v);
	return v * share;
}

/// The dot product, added from z to x
[[nodiscard]] constexpr float DotZyx(const glm::vec3& a, const glm::vec3& b)
{
	return Dot<SumOrder::Zyx>(a, b);
}

/// The length, with the squares added from z to x
[[nodiscard]] inline float LengthZyx(const glm::vec3& v)
{
	return Length<SumOrder::Zyx>(v);
}

} // namespace openblack::x87
