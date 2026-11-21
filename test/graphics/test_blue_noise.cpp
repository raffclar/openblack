/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <numeric>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/BlueNoise.h"

using namespace openblack::graphics;

namespace
{
constexpr uint32_t k_Side = 32;
} // namespace

TEST(BlueNoise, EachRankIsInTheSquareOneTime)
{
	const auto ranks = blue_noise::Ranks(k_Side);
	ASSERT_EQ(ranks.size(), static_cast<size_t>(k_Side) * k_Side);
	auto sorted = ranks;
	std::ranges::sort(sorted);
	std::vector<uint16_t> expected(sorted.size());
	std::iota(expected.begin(), expected.end(), uint16_t {0});
	EXPECT_EQ(sorted, expected);
}

TEST(BlueNoise, EachCallGivesTheSameSquare)
{
	EXPECT_EQ(blue_noise::Ranks(k_Side), blue_noise::Ranks(k_Side));
}

TEST(BlueNoise, TheLowRanksSpreadEvenly)
{
	// The quarter of the texels with the lowest ranks: each block of 4 x 4 texels has about 4 of them. White noise
	// gives blocks with no point or with many points.
	const auto ranks = blue_noise::Ranks(k_Side);
	const auto quarter = ranks.size() / 4;
	constexpr uint32_t k_Block = 4;
	int worst = 0;
	for (uint32_t by = 0; by < k_Side; by += k_Block)
	{
		for (uint32_t bx = 0; bx < k_Side; bx += k_Block)
		{
			int points = 0;
			for (uint32_t y = by; y < by + k_Block; ++y)
			{
				for (uint32_t x = bx; x < bx + k_Block; ++x)
				{
					points += ranks.at((y * k_Side) + x) < quarter ? 1 : 0;
				}
			}
			worst = std::max(worst, std::abs(points - 4));
		}
	}
	EXPECT_LE(worst, 2);
}

TEST(BlueNoise, NeighbourTexelsAreFarApart)
{
	// The mean difference of the ranks of neighbour texels, as a part of all the ranks. White noise gives 1/3.
	const auto ranks = blue_noise::Ranks(k_Side);
	double sum = 0.0;
	for (uint32_t y = 0; y < k_Side; ++y)
	{
		for (uint32_t x = 0; x < k_Side; ++x)
		{
			const int rank = ranks.at((y * k_Side) + x);
			sum += std::abs(rank - static_cast<int>(ranks.at((y * k_Side) + ((x + 1) % k_Side))));
			sum += std::abs(rank - static_cast<int>(ranks.at((((y + 1) % k_Side) * k_Side) + x)));
		}
	}
	const double mean = sum / (2.0 * static_cast<double>(ranks.size()) * static_cast<double>(ranks.size()));
	EXPECT_GT(mean, 0.4);
}

TEST(BlueNoise, TheBytesUseAllTheValuesEvenly)
{
	const auto ranks = blue_noise::Ranks(k_Side);
	const auto bytes = blue_noise::Bytes(ranks);
	ASSERT_EQ(bytes.size(), ranks.size());
	std::vector<int> counts(256, 0);
	for (const auto byte : bytes)
	{
		++counts.at(byte);
	}
	// 1024 texels: each of the 256 values is in the square 4 times
	EXPECT_TRUE(std::ranges::all_of(counts, [](int count) { return count == 4; }));
	EXPECT_TRUE(blue_noise::Bytes({}).empty());
	EXPECT_TRUE(blue_noise::Ranks(0).empty());
}
