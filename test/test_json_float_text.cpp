/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The inspector writes a float as the double of its shortest text of six to nine digits

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <array>
#include <bit>
#include <limits>
#include <random>

#include <gtest/gtest.h>

#include "ECS/Reflection/ComponentJsonCodec.h"

namespace
{
/// The value that the text of the C library gives: the fewest digits from six to nine that read back as the float
double ShortestByPrintf(float value)
{
	if (!std::isfinite(value))
	{
		return static_cast<double>(value);
	}
	std::array<char, 32> text {};
	for (int precision = 6; precision <= 9; ++precision)
	{
		std::snprintf(text.data(), text.size(), "%.*g", precision, static_cast<double>(value));
		if (std::strtof(text.data(), nullptr) == value)
		{
			return std::strtod(text.data(), nullptr);
		}
	}
	return static_cast<double>(value);
}

void ExpectSameBits(float value)
{
	const auto expected = ShortestByPrintf(value);
	const auto actual = openblack::ecs::reflection::detail::Shortest(value);
	EXPECT_EQ(std::bit_cast<uint64_t>(actual), std::bit_cast<uint64_t>(expected))
	    << "float bits " << std::bit_cast<uint32_t>(value);
}
} // namespace

TEST(JsonFloatText, TheShortestTextIsTheTextOfTheCLibraryForEdgeValues)
{
	constexpr std::array k_Values {0.0f,
	                               -0.0f,
	                               1.0f,
	                               0.1f,
	                               0.5f,
	                               1.0f / 3.0f,
	                               2480.0f,
	                               16777216.0f,
	                               16777217.0f,
	                               1e-7f,
	                               123456.789f,
	                               std::numeric_limits<float>::min(),
	                               std::numeric_limits<float>::denorm_min(),
	                               std::numeric_limits<float>::max(),
	                               std::numeric_limits<float>::lowest(),
	                               std::numeric_limits<float>::epsilon()};
	for (const auto value : k_Values)
	{
		ExpectSameBits(value);
	}
	EXPECT_TRUE(std::isnan(openblack::ecs::reflection::detail::Shortest(std::numeric_limits<float>::quiet_NaN())));
	EXPECT_EQ(openblack::ecs::reflection::detail::Shortest(std::numeric_limits<float>::infinity()),
	          std::numeric_limits<double>::infinity());
}

TEST(JsonFloatText, TheShortestTextIsTheTextOfTheCLibraryForManyBitPatterns)
{
	// Each bit pattern of a float, in steps, and random patterns
	for (uint64_t bits = 0; bits <= 0xFFFFFFFFull; bits += 65521)
	{
		ExpectSameBits(std::bit_cast<float>(static_cast<uint32_t>(bits)));
	}
	std::mt19937 random(7);
	for (int i = 0; i < 100000; ++i)
	{
		ExpectSameBits(std::bit_cast<float>(static_cast<uint32_t>(random())));
	}
	// Values of the game's scale: small whole numbers and their halves, tenths and hundredths
	for (int i = -20000; i <= 20000; ++i)
	{
		ExpectSameBits(static_cast<float>(i) * 0.01f);
		ExpectSameBits(static_cast<float>(i) / 10.0f);
	}
}
