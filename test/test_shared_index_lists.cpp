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
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "3D/SharedIndexLists.h"

namespace
{
/// Stands in for an index page: the indices written into it
struct FakePage
{
	explicit FakePage(uint32_t capacity)
	    : indices(capacity, 0xFFFF)
	{
	}
	void Write(uint32_t first, std::span<const uint16_t> run) const
	{
		ASSERT_LE(first + run.size(), indices.size());
		std::ranges::copy(run, indices.begin() + first);
		++writes;
	}
	mutable std::vector<uint16_t> indices;
	mutable int writes {0};
};

struct CountingMaker
{
	int* made;
	std::unique_ptr<FakePage> operator()(uint32_t capacity) const
	{
		++*made;
		return std::make_unique<FakePage>(capacity);
	}
};

using Lists = openblack::SharedIndexLists<FakePage>;

/// The indices of a run, as its page holds them
std::vector<uint16_t> Read(const Lists::Range& range)
{
	const auto first = range.page->indices.begin() + range.first;
	return {first, first + range.count};
}
} // namespace

TEST(SharedIndexLists, TheSameListIsWrittenOnceAndShared)
{
	Lists lists(64);
	int made = 0;
	const auto first = lists.Get({0, 1, 2, 2, 1, 3}, CountingMaker {&made});
	const auto second = lists.Get({0, 1, 2, 2, 1, 3}, CountingMaker {&made});
	ASSERT_NE(first.page, nullptr);
	EXPECT_EQ(first.page, second.page);
	EXPECT_EQ(first.first, second.first);
	EXPECT_EQ(first.page->writes, 1);
	EXPECT_EQ(made, 1);
	EXPECT_EQ(lists.Size(), 1u);
	EXPECT_EQ(Read(first), (std::vector<uint16_t> {0, 1, 2, 2, 1, 3}));
}

TEST(SharedIndexLists, DifferentListsTakeRunsOfOnePage)
{
	Lists lists(64);
	int made = 0;
	const auto first = lists.Get({0, 1, 2}, CountingMaker {&made});
	// The same triangle turned the other way is another list
	const auto second = lists.Get({0, 2, 1}, CountingMaker {&made});
	EXPECT_EQ(made, 1);
	EXPECT_EQ(lists.PageCount(), 1u);
	EXPECT_EQ(lists.Size(), 2u);
	EXPECT_EQ(first.page, second.page);
	EXPECT_EQ(first.first, 0u);
	EXPECT_EQ(second.first, 3u);
	EXPECT_EQ(Read(first), (std::vector<uint16_t> {0, 1, 2}));
	EXPECT_EQ(Read(second), (std::vector<uint16_t> {0, 2, 1}));
}

TEST(SharedIndexLists, AListThatDoesNotFitStartsANewPage)
{
	Lists lists(8);
	int made = 0;
	const auto first = lists.Get({0, 1, 2, 3, 4, 5}, CountingMaker {&made});
	const auto second = lists.Get({6, 7, 8}, CountingMaker {&made});
	const auto third = lists.Get({9, 10}, CountingMaker {&made});
	EXPECT_EQ(made, 2);
	EXPECT_EQ(lists.PageCount(), 2u);
	EXPECT_NE(first.page, second.page);
	EXPECT_EQ(second.first, 0u);
	// A list that fits in the last page goes there
	EXPECT_EQ(third.page, second.page);
	EXPECT_EQ(third.first, 3u);
	EXPECT_EQ(Read(first), (std::vector<uint16_t> {0, 1, 2, 3, 4, 5}));
	EXPECT_EQ(Read(second), (std::vector<uint16_t> {6, 7, 8}));
	EXPECT_EQ(Read(third), (std::vector<uint16_t> {9, 10}));
}

TEST(SharedIndexLists, ManyListsTakeFewPages)
{
	// A land makes more than a thousand lists as the camera moves. The renderer has a limit of buffers.
	Lists lists(1u << 10);
	int made = 0;
	const auto first = lists.Get({7}, CountingMaker {&made});
	for (uint16_t i = 0; i < 2000; ++i)
	{
		static_cast<void>(lists.Get({i, i, i}, CountingMaker {&made}));
	}
	EXPECT_EQ(lists.Size(), 2001u);
	// 6001 indices in pages of 1024 indices
	EXPECT_EQ(made, 6);
	const auto again = lists.Get({7}, CountingMaker {&made});
	EXPECT_EQ(first.page, again.page);
	EXPECT_EQ(first.first, again.first);
	EXPECT_EQ(Read(again), (std::vector<uint16_t> {7}));
}

TEST(SharedIndexLists, AnEmptyListTakesNoPage)
{
	// A block of open sea only has no triangles, and the graphics library cannot draw an empty run
	Lists lists(64);
	int made = 0;
	const auto empty = lists.Get({}, CountingMaker {&made});
	EXPECT_EQ(empty.page, nullptr);
	EXPECT_EQ(empty.count, 0u);
	EXPECT_EQ(made, 0);
	EXPECT_EQ(lists.Size(), 0u);
}

TEST(SharedIndexLists, EachListIsVisitedWithItsIndicesAndItsRun)
{
	// The land writes the indices of each list into its page again after a restore of the graphics device
	Lists lists(64);
	int made = 0;
	const auto first = lists.Get({4, 5, 6}, CountingMaker {&made});
	const auto second = lists.Get({0, 1, 2}, CountingMaker {&made});
	std::vector<std::pair<std::vector<uint16_t>, uint32_t>> visited;
	lists.ForEach([&visited](std::span<const uint16_t> indices, const Lists::Range& range) {
		visited.emplace_back(std::vector<uint16_t>(indices.begin(), indices.end()), range.first);
	});
	ASSERT_EQ(visited.size(), 2u);
	EXPECT_EQ(visited[0].first, (std::vector<uint16_t> {0, 1, 2}));
	EXPECT_EQ(visited[0].second, second.first);
	EXPECT_EQ(visited[1].first, (std::vector<uint16_t> {4, 5, 6}));
	EXPECT_EQ(visited[1].second, first.first);
}
