/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The single queue of blended things (src/Graphics/ZSort.h) against the original's: far to near, stable on equal
// keys, the new entry dropped when full, one drain a frame, the key and the rain's packed user data.

#include <cmath>

#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/ZSort.h"

using namespace openblack::graphics;

namespace
{
std::vector<int> Drained(zsort::Queue<int>& queue)
{
	std::vector<int> order;
	for (const auto& entry : queue.Drain())
	{
		order.push_back(*entry.item);
	}
	return order;
}
} // namespace

TEST(ZSorter, farToNearAndStable)
{
	// a new entry goes before the first strictly smaller key, so equal keys keep their arrival order
	zsort::Queue<int> queue;
	queue.Begin();
	queue.Submit(0, 10.0f);
	queue.Submit(1, 30.0f);
	queue.Submit(2, 10.0f);
	queue.Submit(3, 20.0f);
	queue.Submit(4, 30.0f);
	queue.Submit(5, 0.0f); // the key-0 callers are drawn last
	queue.Submit(6, 10.0f);
	EXPECT_EQ(Drained(queue), (std::vector<int> {1, 4, 3, 0, 2, 6, 5}));
}

TEST(ZSorter, drainedOnceAFrame)
{
	// a drain sets a flag; the frame's end drains only while it is clear, the frame's start clears it
	zsort::Queue<int> queue;
	queue.Begin();
	queue.Submit(7, 1.0f);
	EXPECT_EQ(Drained(queue).size(), 1u);
	EXPECT_TRUE(Drained(queue).empty());
	queue.Begin();
	EXPECT_TRUE(queue.Empty());
	EXPECT_TRUE(Drained(queue).empty());
}

TEST(ZSorter, fullDropsTheNewOne)
{
	// with 0x800 entries the new one is lost, even if it is the farthest
	zsort::Queue<int> queue;
	queue.Begin();
	for (int i = 0; i < static_cast<int>(zsort::k_Capacity); ++i)
	{
		EXPECT_TRUE(queue.Submit(i, 1.0f));
	}
	EXPECT_FALSE(queue.Submit(-1, 1000.0f));
	EXPECT_EQ(queue.Size(), zsort::k_Capacity);
	EXPECT_EQ(queue.Dropped(), 1u);
	const auto order = Drained(queue);
	EXPECT_EQ(order.front(), 0);
	EXPECT_EQ(order.back(), static_cast<int>(zsort::k_Capacity) - 1);
}

TEST(ZSorter, userDataAndNaN)
{
	zsort::Queue<int> queue;
	queue.Begin();
	queue.Submit(0, 5.0f, 0x12345u);
	// a compare with a NaN counts as "smaller": a new NaN goes before the 5 (cur 5 against NaN is unordered), and then
	// the new 1 goes before the NaN (cur NaN against 1 is unordered too), ahead of the far 5
	queue.Submit(1, std::numeric_limits<float>::quiet_NaN());
	queue.Submit(2, 1.0f);
	const auto entries = queue.Drain();
	ASSERT_EQ(entries.size(), 3u);
	EXPECT_EQ(*entries[0].item, 2);
	EXPECT_EQ(*entries[1].item, 1);
	EXPECT_EQ(*entries[2].item, 0);
	EXPECT_EQ(entries[2].user, 0x12345u);
	EXPECT_EQ(entries[2].key, 5.0f);
}

TEST(ZSorter, key)
{
	// the key: the squared distance, not the distance
	const glm::vec3 camera(1.0f, 2.0f, 3.0f);
	EXPECT_FLOAT_EQ(zsort::Key(glm::vec3(4.0f, 6.0f, 15.0f), camera), 9.0f + 16.0f + 144.0f);
	EXPECT_FLOAT_EQ(zsort::Key(glm::vec3(4.0f, 6.0f, 15.0f), camera, zsort::SumOrder::XZY), 169.0f);
	// the two orders round differently in float: x^2 = 1, y^2 ~ 1e-6, z^2 = 2^24 (one ulp there is 2).
	// (1 + 1e-6) + 2^24 is just over 2^24 + 1 and rounds up; (1 + 2^24) is a tie that rounds to even, then + 1e-6 is lost
	const glm::vec3 point(1.0f, 1e-3f, 4096.0f);
	EXPECT_EQ(zsort::Key(point, glm::vec3(0.0f)), 16777218.0f);
	EXPECT_EQ(zsort::Key(point, glm::vec3(0.0f), zsort::SumOrder::XZY), 16777216.0f);
}

TEST(ZSorter, rainUser)
{
	// PackRainUser: alpha x 65536 + 256 trunc(z / 80) + trunc(x / 80), read back by UnpackRainUser
	const auto user = zsort::PackRainUser(1680.0f, 2400.0f, 0x58);
	EXPECT_EQ(user, 0x58u * 65536u + 30u * 256u + 21u);
	const auto unpacked = zsort::UnpackRainUser(user);
	EXPECT_EQ(unpacked.tileX, 21);
	EXPECT_EQ(unpacked.tileZ, 30);
	EXPECT_EQ(unpacked.alpha, 0x58);
	// truncated: 159.9 / 80 is tile 1
	EXPECT_EQ(zsort::UnpackRainUser(zsort::PackRainUser(159.9f, 0.0f, 0)).tileX, 1);
}
