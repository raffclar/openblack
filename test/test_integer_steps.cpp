/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <limits>
#include <type_traits>

#include <gtest/gtest.h>

#include "Common/IntegerSteps.h"

using openblack::ToIndex;
using openblack::WholeQuotient;

TEST(IntegerSteps, ToIndexConvertsTheIntResult)
{
	constexpr int32_t k_Side = 17;
	EXPECT_EQ(ToIndex((3 * k_Side) + 4), size_t {55});
	static_assert(ToIndex(int32_t {7}) == size_t {7});
	static_assert(std::is_same_v<decltype(ToIndex(uint8_t {1})), size_t>);
}

TEST(IntegerSteps, ToIndexKeepsTheIntSum)
{
	// The sum wraps in 32 bits before the conversion, as it did before the helper
	const uint32_t large = std::numeric_limits<uint32_t>::max();
	EXPECT_EQ(ToIndex(large + 1u), size_t {0});
}

TEST(IntegerSteps, WholeQuotientDropsTheRemainder)
{
	EXPECT_EQ(WholeQuotient(7, 2), 3);
	EXPECT_EQ(WholeQuotient(-7, 2), -3);
	EXPECT_EQ(WholeQuotient(1000u, 30u), 33u);
	EXPECT_EQ(static_cast<float>(WholeQuotient(2, 3)), 0.0f);
	static_assert(std::is_same_v<decltype(WholeQuotient(uint32_t {1}, int32_t {1})), uint32_t>);
	static_assert(std::is_same_v<decltype(WholeQuotient(uint8_t {9}, uint8_t {2})), int>);
}
