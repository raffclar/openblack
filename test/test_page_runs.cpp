/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>

#include <gtest/gtest.h>

#include "Graphics/PageRuns.h"

using openblack::graphics::PageRuns;

TEST(PageRuns, RunsFollowEachOtherFromTheStartOfThePage)
{
	PageRuns runs(100, 1);
	EXPECT_EQ(runs.Take(10), std::optional<uint32_t> {0});
	EXPECT_EQ(runs.Take(20), std::optional<uint32_t> {10});
	EXPECT_EQ(runs.Take(70), std::optional<uint32_t> {30});
	EXPECT_EQ(runs.Used(), 100U);
	// The page is full
	EXPECT_EQ(runs.Take(1), std::nullopt);
}

TEST(PageRuns, ARunStartsAtAMultipleOfTheAlignment)
{
	PageRuns runs(16, 2);
	EXPECT_EQ(runs.Take(3), std::optional<uint32_t> {0});
	EXPECT_EQ(runs.Take(3), std::optional<uint32_t> {4});
	EXPECT_EQ(runs.Take(2), std::optional<uint32_t> {8});
	EXPECT_EQ(runs.Used(), 10U);
	EXPECT_EQ(runs.Length(5), 6U);
}

TEST(PageRuns, ACapacityThatIsNotAMultipleOfTheAlignmentLosesItsLastElements)
{
	const PageRuns runs(15, 2);
	EXPECT_EQ(runs.Capacity(), 14U);
}

TEST(PageRuns, AFreeRunGoesToTheNextRunThatFits)
{
	PageRuns runs(100, 1);
	ASSERT_EQ(runs.Take(10), std::optional<uint32_t> {0});
	ASSERT_EQ(runs.Take(10), std::optional<uint32_t> {10});
	ASSERT_EQ(runs.Take(10), std::optional<uint32_t> {20});
	runs.Give(10, 10);
	// A longer run does not fit in the gap, a shorter run does
	EXPECT_EQ(runs.Take(15), std::optional<uint32_t> {30});
	EXPECT_EQ(runs.Take(4), std::optional<uint32_t> {10});
	EXPECT_EQ(runs.Take(6), std::optional<uint32_t> {14});
}

TEST(PageRuns, FreeRunsJoinTheirNeighbours)
{
	PageRuns runs(30, 1);
	ASSERT_EQ(runs.Take(10), std::optional<uint32_t> {0});
	ASSERT_EQ(runs.Take(10), std::optional<uint32_t> {10});
	ASSERT_EQ(runs.Take(10), std::optional<uint32_t> {20});
	runs.Give(0, 10);
	runs.Give(20, 10);
	EXPECT_EQ(runs.FreeRunCount(), 2U);
	// The middle run joins the run before it and the run after it
	runs.Give(10, 10);
	EXPECT_EQ(runs.FreeRunCount(), 1U);
	EXPECT_TRUE(runs.Empty());
	EXPECT_EQ(runs.Take(30), std::optional<uint32_t> {0});
}

TEST(PageRuns, AnEmptyRunTakesNothing)
{
	PageRuns runs(10, 1);
	EXPECT_EQ(runs.Take(0), std::nullopt);
	EXPECT_TRUE(runs.Empty());
}
