/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game's float to integer conversion on each target. Each expected value is the result of the game's own
// conversion on its 64-bit path, from the oracle (test/oracle/data/ftol_portable.txt has these inputs and more).

#include <cstdint>

#include <bit>
#include <limits>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/X87.h"

namespace x87 = openblack::x87;
namespace map_coords = openblack::map_coords;

namespace
{
constexpr uint32_t Bits(int32_t value)
{
	return std::bit_cast<uint32_t>(value);
}

constexpr uint32_t FromFloat(float value)
{
	return Bits(x87::ToInteger(value));
}

constexpr uint32_t FromDouble(double value)
{
	return Bits(x87::ToInteger(value));
}

// The conversion is constexpr, thus the compiler gives the same result as the code at run time
static_assert(FromFloat(1.9f) == 1u);
static_assert(FromFloat(-1.9f) == 0xFFFFFFFFu);
static_assert(FromFloat(8388608.0f * 6553.6f) == 0xCCCCD000u);
static_assert(FromFloat(2147483648.0f) == 0x80000000u);
static_assert(FromFloat(std::numeric_limits<float>::infinity()) == 0u);
static_assert(map_coords::FtoL(3.0e9f) == static_cast<int32_t>(0xB2D05E00u));
} // namespace

TEST(FloatToInteger, InRangeTruncatesTowardsZero)
{
	EXPECT_EQ(FromFloat(0.0f), 0u);
	EXPECT_EQ(FromFloat(-0.0f), 0u);
	EXPECT_EQ(FromFloat(0.5f), 0u);
	EXPECT_EQ(FromFloat(-0.5f), 0u);
	EXPECT_EQ(FromFloat(0.99999994f), 0u);
	EXPECT_EQ(FromFloat(1.9f), 1u);
	EXPECT_EQ(FromFloat(-1.9f), 0xFFFFFFFFu);
	EXPECT_EQ(FromFloat(2.5f), 2u);
	EXPECT_EQ(FromFloat(-2.5f), 0xFFFFFFFEu);
	EXPECT_EQ(FromFloat(3.5f), 3u);
	EXPECT_EQ(FromFloat(123456.7f), 0x0001E240u);
	EXPECT_EQ(FromFloat(-123456.7f), 0xFFFE1DC0u);
	EXPECT_EQ(FromFloat(8388607.5f), 0x007FFFFFu);
	EXPECT_EQ(FromFloat(std::bit_cast<float>(0x00000001u)), 0u);
	EXPECT_EQ(FromFloat(std::bit_cast<float>(0x80000001u)), 0u);
}

TEST(FloatToInteger, EdgesOfTheThirtyTwoBitRange)
{
	EXPECT_EQ(FromFloat(2147483520.0f), 0x7FFFFF80u); // the largest float below 2^31
	EXPECT_EQ(FromDouble(2147483647.0), 0x7FFFFFFFu); // 2^31 - 1
	EXPECT_EQ(FromDouble(2147483647.9), 0x7FFFFFFFu);
	EXPECT_EQ(FromFloat(-2147483648.0f), 0x80000000u); // -2^31
	EXPECT_EQ(FromDouble(-2147483648.9), 0x80000000u);
	EXPECT_EQ(FromFloat(2147483648.0f), 0x80000000u); // 2^31 wraps
	EXPECT_EQ(FromDouble(2147483648.5), 0x80000000u);
	EXPECT_EQ(FromDouble(-2147483649.0), 0x7FFFFFFFu);
	EXPECT_EQ(FromFloat(-2147483904.0f), 0x7FFFFF00u); // the next float below -2^31
}

TEST(FloatToInteger, OutOfRangeGivesTheLowThirtyTwoBits)
{
	EXPECT_EQ(FromFloat(8388608.0f * 6553.6f), 0xCCCCD000u);
	EXPECT_EQ(FromFloat(54975582208.0f), 0xCCCCD000u);
	EXPECT_EQ(FromFloat(-54975582208.0f), 0x33333000u);
	EXPECT_EQ(FromFloat(4294967040.0f), 0xFFFFFF00u); // the largest float below 2^32
	EXPECT_EQ(FromFloat(4294967296.0f), 0u);          // 2^32
	EXPECT_EQ(FromFloat(4294967808.0f), 0x00000200u); // 2^32 + 512
	EXPECT_EQ(FromFloat(-4294967808.0f), 0xFFFFFE00u);
	EXPECT_EQ(FromDouble(4294967301.0), 5u); // 2^32 + 5
	EXPECT_EQ(FromDouble(-4294967301.0), 0xFFFFFFFBu);
	EXPECT_EQ(FromDouble(1099511627783.0), 7u); // 2^40 + 7
	EXPECT_EQ(FromFloat(3.0e9f), 0xB2D05E00u);
	EXPECT_EQ(FromFloat(-3.0e9f), 0x4D2FA200u);
	EXPECT_EQ(FromFloat(1.0e10f), 0x540BE400u);
	EXPECT_EQ(FromFloat(-1.0e10f), 0xABF41C00u);
}

TEST(FloatToInteger, EdgesOfTheSixtyFourBitRange)
{
	EXPECT_EQ(FromDouble(9223372036854774784.0), 0xFFFFFC00u); // the largest double below 2^63
	EXPECT_EQ(FromDouble(-9223372036854774784.0), 0x00000400u);
	EXPECT_EQ(FromFloat(9223371487098961920.0f), 0u); // the largest float below 2^63
	EXPECT_EQ(FromFloat(9223372036854775808.0f), 0u); // 2^63
	EXPECT_EQ(FromFloat(-9223372036854775808.0f), 0u);
	EXPECT_EQ(FromFloat(-9223373136366403584.0f), 0u);
	EXPECT_EQ(FromFloat(1.0e19f), 0u);
	EXPECT_EQ(FromFloat(std::numeric_limits<float>::max()), 0u);
	EXPECT_EQ(FromFloat(std::numeric_limits<float>::lowest()), 0u);
	EXPECT_EQ(FromDouble(1.0e300), 0u);
}

TEST(FloatToInteger, NanAndInfinityGiveZero)
{
	EXPECT_EQ(FromFloat(std::numeric_limits<float>::quiet_NaN()), 0u);
	EXPECT_EQ(FromFloat(std::bit_cast<float>(0xFFC00000u)), 0u);
	EXPECT_EQ(FromFloat(std::bit_cast<float>(0x7F800001u)), 0u);
	EXPECT_EQ(FromFloat(std::numeric_limits<float>::infinity()), 0u);
	EXPECT_EQ(FromFloat(-std::numeric_limits<float>::infinity()), 0u);
	EXPECT_EQ(FromDouble(std::numeric_limits<double>::quiet_NaN()), 0u);
	EXPECT_EQ(FromDouble(-std::numeric_limits<double>::infinity()), 0u);
}

TEST(FloatToInteger, MapCoordsUseTheSameConversion)
{
	EXPECT_EQ(Bits(map_coords::FtoL(8388608.0f * 6553.6f)), 0xCCCCD000u);
	EXPECT_EQ(Bits(map_coords::FtoL(std::numeric_limits<float>::quiet_NaN())), 0u);
	EXPECT_EQ(Bits(map_coords::FtoL(-1.9f)), 0xFFFFFFFFu);
}
