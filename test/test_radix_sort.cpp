/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "Common/RadixSort.h"

using namespace openblack;

namespace
{
/// Keys as the particle order makes them: a distance in the high half and the place of the thing in the low half
std::vector<uint64_t> DepthKeys(size_t count, uint32_t seed, uint32_t depthRange)
{
	std::mt19937 random(seed);
	std::uniform_int_distribution<uint32_t> depth(0, depthRange);
	std::vector<uint64_t> keys;
	for (size_t i = 0; i < count; ++i)
	{
		const uint32_t inverted = 0xFFFFFFFFu - depth(random);
		keys.push_back((static_cast<uint64_t>(inverted) << 32u) | i);
	}
	return keys;
}
} // namespace

TEST(RadixSort, GivesTheSameOrderAsASortByComparisons)
{
	std::vector<uint64_t> scratch;
	for (const size_t count : {size_t {0}, size_t {1}, size_t {63}, size_t {64}, size_t {1000}, size_t {20000}})
	{
		for (const uint32_t range : {0u, 3u, 1000u, 0xFFFFFFFFu})
		{
			auto keys = DepthKeys(count, static_cast<uint32_t>(count) + range, range);
			auto expected = keys;
			std::ranges::sort(expected);
			radix_sort::Sort(keys, scratch);
			EXPECT_EQ(keys, expected) << count << " keys, depths up to " << range;
		}
	}
}

TEST(RadixSort, SortsKeysThatDifferOnlyInTheirHighestByte)
{
	std::vector<uint64_t> keys;
	for (uint64_t i = 0; i < 300; ++i)
	{
		keys.push_back(((299 - i) % 256) << 56u);
	}
	auto expected = keys;
	std::ranges::sort(expected);
	std::vector<uint64_t> scratch;
	radix_sort::Sort(keys, scratch);
	EXPECT_EQ(keys, expected);
}
