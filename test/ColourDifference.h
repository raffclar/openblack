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

#include <array>

/// The measures that the tests use to show that two colours look different
namespace openblack::test_colour
{

/// The light of one part of a colour, from 0 to 1, as the sRGB curve has it
inline double LinearPart(uint32_t part)
{
	const auto value = static_cast<double>(part) / 255.0;
	return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
}

/// The relative luminance of a colour, 0 for black and 1 for white
inline double Luminance(uint32_t argb)
{
	return (0.2126 * LinearPart((argb >> 16u) & 0xFFu)) + (0.7152 * LinearPart((argb >> 8u) & 0xFFu)) +
	       (0.0722 * LinearPart(argb & 0xFFu));
}

/// The contrast of a colour with black, from 1 to 21
inline double ContrastWithBlack(uint32_t argb)
{
	return (Luminance(argb) + 0.05) / 0.05;
}

/// A colour in the CIE L*a*b* space, where the distance between two colours is near how different they look
inline std::array<double, 3> Lab(uint32_t argb)
{
	const auto r = LinearPart((argb >> 16u) & 0xFFu);
	const auto g = LinearPart((argb >> 8u) & 0xFFu);
	const auto b = LinearPart(argb & 0xFFu);
	const auto x = ((0.4124 * r) + (0.3576 * g) + (0.1805 * b)) / 0.95047;
	const auto y = (0.2126 * r) + (0.7152 * g) + (0.0722 * b);
	const auto z = ((0.0193 * r) + (0.1192 * g) + (0.9505 * b)) / 1.08883;
	const auto f = [](double t) { return t > 216.0 / 24389.0 ? std::cbrt(t) : ((24389.0 / 27.0 * t) + 16.0) / 116.0; };
	return {(116.0 * f(y)) - 16.0, 500.0 * (f(x) - f(y)), 200.0 * (f(y) - f(z))};
}

/// The CIE L*a*b* difference of two colours
inline double Difference(uint32_t first, uint32_t second)
{
	const auto a = Lab(first);
	const auto b = Lab(second);
	return std::hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
}

} // namespace openblack::test_colour
