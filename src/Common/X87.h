/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cfloat>
#include <cmath>
#include <cstdint>

#include <bit>
#include <limits>
#include <type_traits>

/// The game's floating point maths, bit for bit.
///
/// The game thread runs its FPU at 24-bit precision. Each add, subtract, multiply, divide and square root rounds its
/// exact result to a float one time. Use these rules to write the game's maths in C++:
///
/// 1. Write a chain of float operations as plain `float` code, in the order of the game's instructions. A float
///    operation in C++ rounds one time, as the FPU does. Do not keep a part of the chain in `double`.
/// 2. Keep a constant that the game holds as a double as a `double`. Use `Mul`, `Add`, `Sub` or `Div` to apply it to
///    a float. These functions round the exact result one time. `float(double(x) * c)` rounds two times, and the
///    result can be one float step wrong.
/// 3. Compare a float with a double constant through `Less`, `LessEqual`, `Greater` or `GreaterEqual`. The compare
///    in double is exact, as the game's compare is.
/// 4. Use `Sin`, `Cos`, `Tan`, `Atan` and `Atan2` for the FPU's sine, cosine, tangent and arctangent. They give a
///    `double`, because the FPU keeps these results wide. The next operation rounds the result. Give the result to
///    `Mul` or `Add`, and do not make it a float first.
/// 5. Use `RoundToFloat` when the game stores a wide value, for example a large integer, as a float. Do not write
///    `static_cast<float>` of a double before a conversion to an integer: MSVC can remove the float step (see Narrow).
/// 6. Use the game's tables where the game uses them: `gutils::InvSqrt` and the distances in GUtilsDistance.h, and
///    `gutils::SinTable`, `gutils::Cos`, `gutils::Sin` and `gutils::LHArcTan` in GUtilsAngle.h. Do not use `std::sqrt`
///    or `std::atan2` in their place.
///
/// Limits:
/// - The FPU keeps a wider exponent than a float. These functions are exact only for results in the normal float range.
/// - `Sin` and the other wide functions use the C library in double. The FPU gives a few more bits, and its sine is not
///   correctly rounded near large multiples of pi. The difference is very rare in the final float.
///
/// The build stops fused multiply-add and reassociation (CMakeLists.txt), so a float expression rounds at each step.
namespace openblack::x87
{

#if defined(FLT_EVAL_METHOD) && FLT_EVAL_METHOD != 0 && FLT_EVAL_METHOD != -1
#error "Float expressions must round at each step. Compile for SSE2 or later, not for the x87 FPU."
#endif

namespace detail
{

/// Rounds a double to a float, and keeps the float.
///
/// MSVC with optimisation can remove a conversion from double to float when a conversion to an integer follows it.
/// Then the integer comes from the double, and it can be one less than the integer of the float. The volatile store
/// stops this optimisation.
[[nodiscard]] constexpr float Narrow(double value)
{
	if (std::is_constant_evaluated())
	{
		return static_cast<float>(value);
	}
	const volatile float narrowed = static_cast<float>(value);
	return narrowed;
}

/// A value as an exact sum of a rounded double and the small error of that rounding
struct Exact
{
	double rounded;
	double error;
};

/// The exact sum of two doubles
[[nodiscard]] inline Exact TwoSum(double a, double b)
{
	const double sum = a + b;
	const double bPart = sum - a;
	const double aPart = sum - bPart;
	return {.rounded = sum, .error = (a - aPart) + (b - bPart)};
}

#if defined(__FMA__) || defined(__ARM_FEATURE_FMA) || defined(_M_ARM64)
/// The exact product of two doubles, from a fused multiply-add of the hardware
[[nodiscard]] inline Exact TwoProduct(double a, double b)
{
	const double product = a * b;
	return {.rounded = product, .error = std::fma(a, b, -product)};
}
#else
/// A double cut into two halves of 26 bits, so that the product of two halves is exact
[[nodiscard]] inline Exact Split(double value)
{
	constexpr double k_Splitter = 134217729.0; // 2^27 + 1
	const double scaled = value * k_Splitter;
	const double high = scaled - (scaled - value);
	return {.rounded = high, .error = value - high};
}

/// The exact product of two doubles, from the products of their halves
[[nodiscard]] inline Exact TwoProduct(double a, double b)
{
	const double product = a * b;
	const auto [aHigh, aLow] = Split(a);
	const auto [bHigh, bLow] = Split(b);
	const double error = (((aHigh * bHigh - product) + aHigh * bLow) + aLow * bHigh) + aLow * bLow;
	return {.rounded = product, .error = error};
}
#endif

/// Rounds an exact value to a float one time.
///
/// The double part rounds to the nearest float, except when it is exactly half-way between two floats. In that case
/// the error part tells which float is nearer to the exact value.
[[nodiscard]] inline float RoundOnce(double rounded, double error)
{
	// The 29 low mantissa bits of the double are the bits that a float does not keep
	constexpr uint64_t k_DroppedMask = (uint64_t {1} << 29) - 1;
	constexpr uint64_t k_HalfWay = uint64_t {1} << 28;
	if (error != 0.0 && (std::bit_cast<uint64_t>(rounded) & k_DroppedMask) == k_HalfWay)
	{
		const double towards = error > 0.0 ? std::numeric_limits<double>::infinity() : -std::numeric_limits<double>::infinity();
		rounded = std::nextafter(rounded, towards);
	}
	return Narrow(rounded);
}

} // namespace detail

/// Rounds a wide value to a float one time
[[nodiscard]] constexpr float RoundToFloat(double value)
{
	return detail::Narrow(value);
}

/// a * b, rounded to a float one time. Two floats need no helper: write a * b in float
[[nodiscard]] inline float Mul(double a, double b)
{
	const auto [product, error] = detail::TwoProduct(a, b);
	return detail::RoundOnce(product, error);
}

/// a + b, rounded to a float one time
[[nodiscard]] inline float Add(double a, double b)
{
	const auto [sum, error] = detail::TwoSum(a, b);
	return detail::RoundOnce(sum, error);
}

/// a - b, rounded to a float one time
[[nodiscard]] inline float Sub(double a, double b)
{
	return Add(a, -b);
}

/// a / b, rounded to a float one time
[[nodiscard]] inline float Div(double a, double b)
{
	const double quotient = a / b;
	// The remainder a - quotient * b is exact, and its sign tells on which side of the true quotient the double is
	const auto [product, error] = detail::TwoProduct(quotient, b);
	const double remainder = (a - product) - error;
	const double side = (remainder > 0.0) == (b > 0.0) ? 1.0 : -1.0;
	return detail::RoundOnce(quotient, remainder == 0.0 ? 0.0 : side);
}

/// The square root of a, rounded to a float one time
[[nodiscard]] inline float Sqrt(double a)
{
	const double root = std::sqrt(a);
	const auto [square, error] = detail::TwoProduct(root, root);
	return detail::RoundOnce(root, (a - square) - error);
}

/// The FPU's sine, kept wide. Give the result to the next operation, which rounds it
[[nodiscard]] inline double Sin(double radians)
{
	return std::sin(radians);
}

/// The FPU's cosine, kept wide
[[nodiscard]] inline double Cos(double radians)
{
	return std::cos(radians);
}

/// The FPU's tangent, kept wide
[[nodiscard]] inline double Tan(double radians)
{
	return std::tan(radians);
}

/// The FPU's arctangent of one value, kept wide
[[nodiscard]] inline double Atan(double value)
{
	return std::atan(value);
}

/// The FPU's arctangent of y / x in the correct quarter of the circle, kept wide
[[nodiscard]] inline double Atan2(double y, double x)
{
	return std::atan2(y, x);
}

namespace detail
{

/// 2 to the power of a wide value, as the FPU's power steps make it. The steps are:
/// 1. The whole number of the value, rounded to the nearest, comes off exactly.
/// 2. The part that is left rounds to a float.
/// 3. The FPU makes 2 to the power of that part, less one, at its full precision.
/// 4. The one goes back, and the sum rounds to a float.
/// 5. The whole number sets the exponent of the sum exactly.
[[nodiscard]] inline float PowerOfTwo(double value)
{
	constexpr double k_Ln2 = 0.6931471805599453;
	const double whole = std::nearbyint(value);
	const float part = Sub(value, whole);
	const float sum = Add(1.0, std::expm1(static_cast<double>(part) * k_Ln2));
	return Narrow(std::ldexp(static_cast<double>(sum), static_cast<int>(whole)));
}

} // namespace detail

/// The C library's power of the game, for a base more than 0. The library keeps the FPU at 24-bit precision. The FPU
/// makes the exponent times the base's logarithm at its full precision, then 2 to the power of that (see PowerOfTwo).
///
/// Limits: this function uses the base's logarithm in double. A zero, a negative or an infinite base, and a result out
/// of the float range, are not the game's.
[[nodiscard]] inline float Pow(float base, double exponent)
{
	return detail::PowerOfTwo(exponent * std::log2(static_cast<double>(base)));
}

/// The game's e to the power of a float, when the game makes it with the FPU's own steps. The value times the FPU's
/// logarithm of e rounds to a float, then the steps of PowerOfTwo make the result.
///
/// Limits: this function uses the logarithm of e in double, not the FPU's wider constant.
[[nodiscard]] inline float Exp(float value)
{
	constexpr double k_Log2E = 1.4426950408889634;
	return detail::PowerOfTwo(static_cast<double>(Mul(value, k_Log2E)));
}

/// value < constant, exact as the game's compare of a float with a double constant
[[nodiscard]] constexpr bool Less(float value, double constant)
{
	return static_cast<double>(value) < constant;
}

/// value <= constant, exact
[[nodiscard]] constexpr bool LessEqual(float value, double constant)
{
	return static_cast<double>(value) <= constant;
}

/// value > constant, exact
[[nodiscard]] constexpr bool Greater(float value, double constant)
{
	return static_cast<double>(value) > constant;
}

/// value >= constant, exact
[[nodiscard]] constexpr bool GreaterEqual(float value, double constant)
{
	return static_cast<double>(value) >= constant;
}

} // namespace openblack::x87
