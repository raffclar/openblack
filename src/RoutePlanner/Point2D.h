/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>

// The LH route planner's 2D point. Metres on the land's x / z. The game runs the FPU at 24 bits, so every intermediate
// is a float: each operation below is one float operation, in the original's order.

namespace openblack::route_planner
{
struct Point2D
{
	float x {0.0f};
	float z {0.0f};

	/// sqrt(x x + z z)
	[[nodiscard]] float GetLength() const { return std::sqrt(x * x + z * z); }
	/// x x + z z
	[[nodiscard]] float LengthSquared() const { return x * x + z * z; }
	/// sqrt((z - b.z)^2 + (x - b.x)^2)
	[[nodiscard]] float DistanceTo(const Point2D& b) const
	{
		const float dx = x - b.x;
		const float dz = z - b.z;
		return std::sqrt(dz * dz + dx * dx);
	}
	/// f = len / GetLength(), x *= f, z *= f
	void SetSize(float length)
	{
		const float f = length / GetLength();
		x = f * x;
		z = f * z;
	}
	/// len = GetLength(), f = 1 / len, x *= f, z *= f; returns len
	float Normalize()
	{
		const float length = GetLength();
		const float f = 1.0f / length;
		x = f * x;
		z = f * z;
		return length;
	}

	/// 0 when x x + z z <= 1e-6; else OctantAtan(-z, x), atan2(x, -z) by octants. The arc tangent keeps its full
	/// precision; the divisions are rounded (24-bit FPU)
	[[nodiscard]] float GetHeading() const
	{
		const float xx = x * x;
		const float zz = z * z;
		const float sum = xx + zz;
		if (!(sum > 1e-6f))
		{
			return 0.0f;
		}
		return static_cast<float>(OctantAtan(-z, x));
	}

	/// atan2(b, a) in four octant pairs
	[[nodiscard]] static double OctantAtan(float a, float b)
	{
		constexpr float k_Pi = 3.14159265358979f;
		constexpr float k_HalfPi = k_Pi * 0.5f;
		// a >= b and -b <= a -> atan(b / a)
		if (!(a < b) && !(-b > a))
		{
			const float q = b / a;
			return std::atan(static_cast<double>(q));
		}
		// b >= a and -a <= b -> pi/2 - atan(a / b)
		if (!(b < a) && !(-a > b))
		{
			const float q = a / b;
			return static_cast<double>(k_HalfPi) - std::atan(static_cast<double>(q));
		}
		// -b >= a and a < b -> atan(b / a) + pi, or - pi when b < 0
		if (!(-b < a) && a < b)
		{
			const float q = b / a;
			const double t = std::atan(static_cast<double>(q));
			return b < 0.0f ? t - static_cast<double>(k_Pi) : t + static_cast<double>(k_Pi);
		}
		// -pi/2 - atan(a / b)
		const float q = a / b;
		return static_cast<double>(-k_HalfPi) - std::atan(static_cast<double>(q));
	}

	bool operator==(const Point2D& other) const = default;
};
} // namespace openblack::route_planner
