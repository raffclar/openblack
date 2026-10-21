/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The helpers for the game's 24-bit float maths. Each expected value comes from an exact rational calculation that
// rounds one time to 24 bits, as the game's FPU does. Most cases put the double result exactly half-way between two
// floats, where a second rounding gives the wrong float.

#include <cmath>
#include <cstdint>

#include <bit>
#include <numbers>

#include <gtest/gtest.h>

#include "Common/X87.h"

namespace x87 = openblack::x87;

namespace
{
uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

double FromBits(uint64_t bits)
{
	return std::bit_cast<double>(bits);
}

/// One plus one float step, 1 + 2^-23
constexpr uint32_t k_OneAndAStep = 0x3F800001;
} // namespace

TEST(X87, AddRoundsOneTime)
{
	// 1 + 2^-24 + 2^-76 is just above the half-way point, so the game gives the next float up. In double, the sum
	// rounds to the half-way point, and then to the even float 1
	const double constant = 0x1.0000000000001p-24; // 2^-24 + 2^-76
	EXPECT_EQ(Bits(x87::Add(1.0f, constant)), k_OneAndAStep);
	EXPECT_EQ(Bits(static_cast<float>(1.0 + constant)), 0x3F800000u);
	EXPECT_EQ(Bits(x87::Sub(1.0f, -constant)), k_OneAndAStep);
}

TEST(X87, MulRoundsOneTime)
{
	// The double nearest to (1 + 2^-24) / 3. Three times it is a little above 1 + 2^-24
	const double third = FromBits(0x3FD555556AAAAAABull);
	EXPECT_EQ(Bits(x87::Mul(3.0f, third)), k_OneAndAStep);
	EXPECT_EQ(Bits(static_cast<float>(3.0 * third)), 0x3F800000u);
}

TEST(X87, DivRoundsOneTime)
{
	// The double nearest to 33 / (1 + 2^-24). The quotient 33 / divisor is a little above 1 + 2^-24
	const double divisor = FromBits(0x40407FFFEF800010ull);
	EXPECT_EQ(Bits(x87::Div(33.0f, divisor)), k_OneAndAStep);
	EXPECT_EQ(Bits(static_cast<float>(33.0 / divisor)), 0x3F800000u);
}

TEST(X87, SqrtRoundsOneTime)
{
	// (1 + 2^-24)^2 plus one double step: its root is a little above 1 + 2^-24
	const double square = FromBits(0x3FF0000020000011ull);
	EXPECT_EQ(Bits(x87::Sqrt(square)), k_OneAndAStep);
	EXPECT_EQ(Bits(static_cast<float>(std::sqrt(square))), 0x3F800000u);
	// The exact square has the root exactly half-way, which rounds to the even float
	EXPECT_EQ(Bits(x87::Sqrt(FromBits(0x3FF0000020000010ull))), 0x3F800000u);
}

TEST(X87, ExactHalfWayRoundsToEven)
{
	EXPECT_EQ(Bits(x87::Add(1.0f, 0x1.0p-24)), 0x3F800000u);
	EXPECT_EQ(Bits(x87::Add(1.0f, 0x1.8p-23)), 0x3F800002u);
}

TEST(X87, OrdinaryValuesMatchOneFloatOperation)
{
	EXPECT_EQ(x87::Mul(0.5f, 3.0f), 1.5f);
	EXPECT_EQ(x87::Mul(0.1f, 0.2f), 0.1f * 0.2f);
	EXPECT_EQ(x87::Add(0.1f, 0.2f), 0.1f + 0.2f);
	EXPECT_EQ(x87::Div(1.0f, 3.0f), 1.0f / 3.0f);
	EXPECT_EQ(x87::Sqrt(2.0f), std::sqrt(2.0f));
	EXPECT_EQ(x87::Mul(-2.0f, 0.25), -0.5f);
	EXPECT_EQ(x87::Div(-1.0f, 4.0), -0.25f);
	EXPECT_EQ(x87::RoundToFloat(16777217.0), 16777216.0f);
}

TEST(X87, CompareWithDoubleConstantIsExact)
{
	// The float 0.1f is above the double 0.1, so a compare in float would call them equal
	EXPECT_TRUE(x87::Greater(0.1f, 0.1));
	EXPECT_FALSE(x87::LessEqual(0.1f, 0.1));
	EXPECT_TRUE(x87::Less(0.15f, 0.15000001));
	EXPECT_TRUE(x87::GreaterEqual(0.5f, 0.5));
}

TEST(X87, WideTrigonometryFeedsTheNextOperation)
{
	EXPECT_EQ(x87::Sin(0.0), 0.0);
	EXPECT_EQ(x87::Cos(0.0), 1.0);
	EXPECT_EQ(x87::Atan2(1.0, 1.0), std::numbers::pi / 4.0);
	EXPECT_EQ(x87::Atan(1.0), std::numbers::pi / 4.0);
	EXPECT_EQ(x87::Tan(0.0), 0.0);
	// The cosine of the float nearest to pi is a little more than -1. The game multiplies the wide value by 255 and
	// rounds one time, which gives exactly 255
	const float pi = std::numbers::pi_v<float>;
	EXPECT_EQ(x87::Mul(std::abs(x87::Cos(pi)), 255.0f), 255.0f);
}

TEST(X87, TheFloatStaysBeforeAnIntegerConversion)
{
	// The wide product is a little under 255, and its float is exactly 255. An optimised build must not take the integer
	// from the wide product
	const float pi = std::numbers::pi_v<float>;
	EXPECT_EQ(static_cast<uint8_t>(x87::Mul(std::abs(x87::Cos(pi)), 255.0f)), 255);
	EXPECT_EQ(static_cast<int32_t>(x87::RoundToFloat(254.99999999999997)), 255);
}

TEST(X87, VillagerTurnStepChain)
{
	// A turn of 2 radians: the size times 2 / pi, doubled, times the turn a millisecond, times 10 milliseconds. Each
	// step rounds to a float
	const double twoOverPi = FromBits(0x3FE45F306DC9C883ull);
	const double turnPerMillisecond = static_cast<double>(0.003f);
	const float scaled = x87::Mul(2.0f, twoOverPi);
	const float doubled = scaled + scaled;
	const float perMillisecond = x87::Mul(doubled, turnPerMillisecond);
	EXPECT_EQ(Bits(perMillisecond), 0x3BFA543Eu);
	EXPECT_EQ(Bits(10.0f * perMillisecond), 0x3D9C74A7u);
}

TEST(X87, ShortWayGivesTheSameFloatAsTheExactWay)
{
	// Products, quotients and roots of many doubles, against the exact value rounded one time
	uint64_t state = 0x9E3779B97F4A7C15u;
	const auto next = [&state]() {
		state ^= state << 13U;
		state ^= state >> 7U;
		state ^= state << 17U;
		return state;
	};
	const auto value = [&next]() {
		// A double between -1024 and 1024, some with fewer mantissa bits, as floats and their products have
		auto wide = (static_cast<double>(next() >> 11U) / 9007199254740992.0 - 0.5) * 2048.0;
		if ((next() & 3U) == 0)
		{
			wide = static_cast<double>(static_cast<float>(wide));
		}
		return wide;
	};
	for (int i = 0; i < 200'000; ++i)
	{
		const double a = value();
		const double b = value();
		const auto [product, productError] = x87::detail::TwoProduct(a, b);
		ASSERT_EQ(Bits(x87::Mul(a, b)), Bits(x87::detail::RoundOnce(product, productError)));
		if (b != 0.0)
		{
			const double quotient = a / b;
			const auto [back, backError] = x87::detail::TwoProduct(quotient, b);
			const double remainder = (a - back) - backError;
			const double side = (remainder > 0.0) == (b > 0.0) ? 1.0 : -1.0;
			ASSERT_EQ(Bits(x87::Div(a, b)), Bits(x87::detail::RoundOnce(quotient, remainder == 0.0 ? 0.0 : side)));
		}
		const double positive = std::abs(a);
		const double root = std::sqrt(positive);
		const auto [square, squareError] = x87::detail::TwoProduct(root, root);
		ASSERT_EQ(Bits(x87::Sqrt(positive)), Bits(x87::detail::RoundOnce(root, (positive - square) - squareError)));
	}
}
