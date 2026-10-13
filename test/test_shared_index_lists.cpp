/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <memory>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "3D/SharedIndexLists.h"

namespace
{
/// Stands in for an index buffer: what it was made from
struct FakeBuffer
{
	std::vector<uint16_t> indices;
};

struct CountingMaker
{
	int* made;
	std::unique_ptr<FakeBuffer> operator()(std::span<const uint16_t> indices) const
	{
		++*made;
		return std::make_unique<FakeBuffer>(FakeBuffer {.indices = {indices.begin(), indices.end()}});
	}
};
} // namespace

TEST(SharedIndexLists, TheSameListIsMadeOnceAndShared)
{
	openblack::SharedIndexLists<FakeBuffer> lists;
	int made = 0;
	const auto& first = lists.Get({0, 1, 2, 2, 1, 3}, CountingMaker {&made});
	const auto& second = lists.Get({0, 1, 2, 2, 1, 3}, CountingMaker {&made});
	EXPECT_EQ(&first, &second);
	EXPECT_EQ(made, 1);
	EXPECT_EQ(lists.Size(), 1u);
	EXPECT_EQ(first.indices, (std::vector<uint16_t> {0, 1, 2, 2, 1, 3}));
}

TEST(SharedIndexLists, DifferentListsGetTheirOwnBuffers)
{
	openblack::SharedIndexLists<FakeBuffer> lists;
	int made = 0;
	const auto& first = lists.Get({0, 1, 2}, CountingMaker {&made});
	// The same triangle turned the other way is another list
	const auto& second = lists.Get({0, 2, 1}, CountingMaker {&made});
	EXPECT_NE(&first, &second);
	EXPECT_EQ(made, 2);
	EXPECT_EQ(lists.Size(), 2u);
	EXPECT_EQ(second.indices, (std::vector<uint16_t> {0, 2, 1}));
}

TEST(SharedIndexLists, ABufferStaysPutAsMoreAreMade)
{
	openblack::SharedIndexLists<FakeBuffer> lists;
	int made = 0;
	const auto* first = &lists.Get({7}, CountingMaker {&made});
	for (uint16_t i = 0; i < 100; ++i)
	{
		static_cast<void>(lists.Get({i, i, i}, CountingMaker {&made}));
	}
	EXPECT_EQ(first, &lists.Get({7}, CountingMaker {&made}));
	EXPECT_EQ(made, 101);
}
