/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Common/X87.h"

/// The game's angle of a vector, bit for bit.
///
/// The game does not use the C library's arctangent of two values. It divides the smaller part by the larger part as a
/// float, takes the FPU's arctangent of that share, and moves the angle into the correct quarter with a double constant.
/// Use these functions where the game uses its own angle of a vector. Use `x87::Atan2` only where the game uses the FPU's
/// arctangent of two values directly.
namespace openblack::x87
{

namespace detail
{
/// A quarter turn and a half turn, as the game keeps them: the floats widened to doubles
inline constexpr double k_QuarterTurn = static_cast<double>(1.5707964f);
inline constexpr double k_HalfTurn = static_cast<double>(3.1415927f);

/// `a < b`, or no order (a NaN), as the FPU's compare flags tell it
[[nodiscard]] constexpr bool LessOrUnordered(float a, float b)
{
	return !(a >= b);
}

/// `a <= b`, or no order (a NaN)
[[nodiscard]] constexpr bool LessEqualOrUnordered(float a, float b)
{
	return !(a > b);
}
} // namespace detail

/// The game's angle of the point (x, y) from the x axis, from -pi to pi. The result is wide when it is the arctangent
/// alone (x is the largest part and positive). In the other cases the move by the constant rounds it to a float.
[[nodiscard]] inline double GameAtan2(float y, float x)
{
	using detail::LessEqualOrUnordered;
	using detail::LessOrUnordered;
	if (!LessOrUnordered(x, y) && LessEqualOrUnordered(-y, x))
	{
		return Atan(y / x);
	}
	if (!LessOrUnordered(y, x) && LessEqualOrUnordered(-x, y))
	{
		return Sub(detail::k_QuarterTurn, Atan(x / y));
	}
	if (!LessOrUnordered(-y, x) && LessOrUnordered(x, y))
	{
		const double angle = Atan(y / x);
		return LessOrUnordered(y, 0.0f) ? Sub(angle, detail::k_HalfTurn) : Add(angle, detail::k_HalfTurn);
	}
	return Sub(-detail::k_QuarterTurn, Atan(x / y));
}

/// The game's heading of a direction on the land: the angle of (-z, x), and 0 when the direction is shorter than the
/// least length the game turns to
[[nodiscard]] inline double GameHeadingOf(float x, float z)
{
	constexpr float k_LeastSquaredLength = 1e-6f;
	if (!((x * x) + (z * z) > k_LeastSquaredLength))
	{
		return 0.0;
	}
	return GameAtan2(x, -z);
}

} // namespace openblack::x87
