/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// The trigonometry, exponentials and logarithms of the world's maths, with the same bits on each build.
///
/// The C libraries of the builds give other results in the last bit: the desktop build uses the Microsoft C runtime and
/// the web build uses musl. Two computers of a network game must make the same world, so the world's maths uses these
/// functions and not the C library's. They do only additions, multiplications, divisions and compares of doubles, each
/// of which rounds one time on every build (the build stops fused multiply-add and reassociation).
///
/// The double functions are the steps of musl: the trigonometry and Expm1 of the freely distributable maths library of
/// Sun Microsystems (fdlibm), and Exp, Log and Log2 of Arm's optimised routines with their tables. Their error is less
/// than one unit in the last place of a double. The float functions round the double
/// result to a float one time, as the game's FPU does when it stores the wide result of its own sine or cosine.
///
/// Limits: the reduction of an angle to a quarter of the circle is exact for angles up to about 2^20 times pi / 2. Larger
/// angles get the same three-part reduction, so their results are the same on each build but less exact. The world never
/// uses such angles.
namespace openblack::portable_math
{

[[nodiscard]] double Sin(double radians);
[[nodiscard]] double Cos(double radians);
[[nodiscard]] double Tan(double radians);
[[nodiscard]] double Atan(double value);
/// The arctangent of y / x in the correct quarter of the circle
[[nodiscard]] double Atan2(double y, double x);

[[nodiscard]] double Acos(double value);
[[nodiscard]] double Exp(double value);
/// e to the power of the value, less one, exact also near 0
[[nodiscard]] double Expm1(double value);
[[nodiscard]] double Log(double value);
[[nodiscard]] double Log2(double value);

/// The double result rounded to a float one time
[[nodiscard]] float Sin(float radians);
[[nodiscard]] float Cos(float radians);
[[nodiscard]] float Tan(float radians);
[[nodiscard]] float Atan(float value);
[[nodiscard]] float Atan2(float y, float x);
[[nodiscard]] float Acos(float value);
[[nodiscard]] float Exp(float value);
[[nodiscard]] float Log(float value);
[[nodiscard]] float Log2(float value);

} // namespace openblack::portable_math
