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

namespace openblack::exact_remainder
{

/// The remainder of value / divisor, with the same bits as std::fmod. It is faster than the C library's fmod when the
/// size of the value is less than 2^24 divisors.
///
/// A double holds the product of the whole number of divisors and the divisor with no rounding, and also the difference
/// between the value and that product. Thus the double steps give the exact remainder. The quotient of the double
/// division can be one too large when the value is very near a whole number of divisors. Then the remainder is less
/// than zero and one divisor is added again, which is also exact. Other values go to std::fmod.
[[nodiscard]] inline float Fmod(float value, float divisor)
{
	constexpr double k_MostDivisors = 16777216.0;
	if (!(divisor > 0.0f) || !std::isfinite(divisor))
	{
		return std::fmod(value, divisor);
	}
	// The remainder of a value less than zero is the remainder of its size, less than zero. The remainder of an infinite
	// value is not a number, and fmod gives it its own sign.
	if (value < 0.0f && std::isfinite(value))
	{
		return -Fmod(-value, divisor);
	}
	if (!(value > 0.0f))
	{
		return std::fmod(value, divisor);
	}
	if (value < divisor)
	{
		return value;
	}
	const double wide = value;
	const double wideDivisor = divisor;
	const double divisors = std::floor(wide / wideDivisor);
	if (!(divisors < k_MostDivisors))
	{
		return std::fmod(value, divisor);
	}
	double remainder = wide - divisors * wideDivisor;
	if (remainder < 0.0)
	{
		remainder += wideDivisor;
	}
	else if (remainder >= wideDivisor)
	{
		remainder -= wideDivisor;
	}
	return static_cast<float>(remainder);
}

} // namespace openblack::exact_remainder
