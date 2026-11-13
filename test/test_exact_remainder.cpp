/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <bit>
#include <limits>
#include <numbers>
#include <random>

#include <gtest/gtest.h>

#include "Common/ExactRemainder.h"

using openblack::exact_remainder::Fmod;

namespace
{
constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;

void ExpectSameBits(float value, float divisor)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(Fmod(value, divisor)), std::bit_cast<uint32_t>(std::fmod(value, divisor)))
	    << value << " " << divisor;
}
} // namespace

TEST(ExactRemainder, RandomValuesGiveTheBitsOfFmod)
{
	std::mt19937 random(7);
	std::uniform_int_distribution<uint32_t> bits;
	for (int i = 0; i < 200000; ++i)
	{
		const float value = std::bit_cast<float>(bits(random));
		ExpectSameBits(value, k_TwoPi);
		ExpectSameBits(value, 32.0f);
		ExpectSameBits(value, std::bit_cast<float>(bits(random) & 0x7FFFFFFFu));
	}
}

TEST(ExactRemainder, ValuesNearAWholeNumberOfDivisorsGiveTheBitsOfFmod)
{
	for (const float divisor : {k_TwoPi, 32.0f, 0.1f, 3.0f})
	{
		for (int whole = 1; whole < 5000000; whole = whole * 3 + 1)
		{
			const float near = static_cast<float>(whole) * divisor;
			for (int step = -3; step <= 3; ++step)
			{
				float value = near;
				for (int n = 0; n < std::abs(step); ++n)
				{
					value = std::nextafter(value, step < 0 ? 0.0f : std::numeric_limits<float>::infinity());
				}
				ExpectSameBits(value, divisor);
				ExpectSameBits(-value, divisor);
			}
		}
	}
}

TEST(ExactRemainder, SpecialValuesGiveTheBitsOfFmod)
{
	constexpr float k_Infinity = std::numeric_limits<float>::infinity();
	for (const float value : {0.0f, -0.0f, 1.0f, -1.0f, k_Infinity, -k_Infinity, std::numeric_limits<float>::max(),
	                          std::numeric_limits<float>::denorm_min(), 1e30f})
	{
		for (const float divisor : {k_TwoPi, -k_TwoPi, k_Infinity, std::numeric_limits<float>::denorm_min(), 1e-30f})
		{
			ExpectSameBits(value, divisor);
		}
	}
	EXPECT_TRUE(std::isnan(Fmod(std::numeric_limits<float>::quiet_NaN(), k_TwoPi)));
	EXPECT_TRUE(std::isnan(Fmod(1.0f, 0.0f)));
}
