/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <numeric>
#include <vector>

#include <Common/FrameStats.h>
#include <gtest/gtest.h>

using namespace openblack::frame_stats;

TEST(FrameStats, EmptyRunIsEmpty)
{
	const auto summary = Summarise({});
	EXPECT_EQ(summary.count, 0u);
	EXPECT_FLOAT_EQ(summary.average, 0.0f);
	EXPECT_FLOAT_EQ(summary.p95, 0.0f);
	EXPECT_FLOAT_EQ(summary.max, 0.0f);
}

TEST(FrameStats, OneFrame)
{
	constexpr std::array k_Times {7.5f};
	const auto summary = Summarise(k_Times);
	EXPECT_EQ(summary.count, 1u);
	EXPECT_FLOAT_EQ(summary.average, 7.5f);
	EXPECT_FLOAT_EQ(summary.p95, 7.5f);
	EXPECT_FLOAT_EQ(summary.max, 7.5f);
}

TEST(FrameStats, HundredFramesTakeTheNinetyFifth)
{
	// 1 ms to 100 ms, shuffled out of order
	std::vector<float> times(100);
	std::iota(times.rbegin(), times.rend(), 1.0f);
	const auto summary = Summarise(times);
	EXPECT_EQ(summary.count, 100u);
	EXPECT_FLOAT_EQ(summary.average, 50.5f);
	EXPECT_FLOAT_EQ(summary.p50, 50.0f);
	EXPECT_FLOAT_EQ(summary.p95, 95.0f);
	EXPECT_FLOAT_EQ(summary.p99, 99.0f);
	EXPECT_FLOAT_EQ(summary.max, 100.0f);
}

TEST(FrameStats, OneSlowFrameInTwentyIsOutsideTheNinetyFifth)
{
	std::vector<float> times(20, 10.0f);
	times.at(3) = 50.0f;
	const auto summary = Summarise(times);
	EXPECT_FLOAT_EQ(summary.p95, 10.0f);
	EXPECT_FLOAT_EQ(summary.max, 50.0f);
	EXPECT_FLOAT_EQ(summary.average, 12.0f);
}

TEST(FrameStats, WindowStartsAgainAfterTaking)
{
	Window window;
	window.Add(4.0f);
	window.Add(6.0f);
	EXPECT_EQ(window.Count(), 2u);
	const auto first = window.Take();
	EXPECT_FLOAT_EQ(first.average, 5.0f);
	EXPECT_EQ(window.Count(), 0u);
	window.Add(9.0f);
	const auto second = window.Take();
	EXPECT_EQ(second.count, 1u);
	EXPECT_FLOAT_EQ(second.max, 9.0f);
}

TEST(FrameStats, HundredFramesTakeTheNinetyNinthAndCountTheSlowOnes)
{
	// 1 ms to 100 ms: the frames of 10 ms and more are slow
	std::vector<float> times(100);
	std::iota(times.begin(), times.end(), 1.0f);
	const auto summary = Summarise(times);
	EXPECT_FLOAT_EQ(summary.p99, 99.0f);
	EXPECT_EQ(summary.slow, 91u);
}

TEST(FrameStats, PeekKeepsTheFrames)
{
	Window window;
	window.Add(12.0f);
	window.Add(2.0f);
	const auto peeked = window.Peek();
	EXPECT_EQ(peeked.count, 2u);
	EXPECT_EQ(peeked.slow, 1u);
	EXPECT_EQ(window.Count(), 2u);
}

TEST(FrameStats, TheCsvHasALineForEachFrame)
{
	const std::array<FrameRecord, 2> frames {
	    FrameRecord {.milliseconds = 3.5f, .processorMilliseconds = 3.25f, .turn = 7, .played = false},
	    FrameRecord {.milliseconds = 12.0f, .processorMilliseconds = 11.5f, .turn = 8, .played = true},
	};
	EXPECT_EQ(ToCsv(frames), "frame,ms,processor_ms,turn,played\n1,3.500,3.250,7,0\n2,12.000,11.500,8,1\n");
}
