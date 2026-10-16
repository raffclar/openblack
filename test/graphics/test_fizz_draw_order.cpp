/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <optional>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "Graphics/FizzDrawOrder.h"

using namespace openblack::graphics::fizz_draw_order;

namespace
{

constexpr entt::entity Thing(uint32_t id)
{
	return static_cast<entt::entity>(id);
}

/// A fake that tells which things fizz
auto FizzingAmong(std::set<entt::entity> fizzing)
{
	return [fizzing = std::move(fizzing)](entt::entity entity) { return fizzing.contains(entity); };
}

} // namespace

TEST(FizzDrawOrder, WithNoFizzingCreatureAllThingsAreInTheFirstSegment)
{
	const std::array list {Thing(1), Thing(2), Thing(3)};
	const auto order = Order::Of(list, FizzingAmong({}));
	EXPECT_TRUE(order.Empty());
	for (const auto thing : list)
	{
		EXPECT_EQ(order.SegmentOf(thing), 0u);
	}
}

TEST(FizzDrawOrder, TheThingsAfterAFizzingCreatureAreInItsSegment)
{
	const std::array list {Thing(1), Thing(2), Thing(3), Thing(4), Thing(5)};
	const auto order = Order::Of(list, FizzingAmong({Thing(2), Thing(4)}));
	EXPECT_FALSE(order.Empty());
	EXPECT_EQ(order.SegmentOf(Thing(1)), 0u);
	// A fizzing creature stays in the segment before it
	EXPECT_EQ(order.SegmentOf(Thing(2)), 0u);
	EXPECT_EQ(order.SegmentOf(Thing(3)), 1u);
	EXPECT_EQ(order.SegmentOf(Thing(4)), 1u);
	EXPECT_EQ(order.SegmentOf(Thing(5)), 2u);
	EXPECT_EQ(order.RankOf(Thing(2)), 0u);
	EXPECT_EQ(order.RankOf(Thing(4)), 1u);
	EXPECT_EQ(order.RankOf(Thing(3)), std::nullopt);
}

TEST(FizzDrawOrder, AThingThatIsNotInTheListIsInTheFirstSegment)
{
	const std::array list {Thing(1), Thing(2)};
	const auto order = Order::Of(list, FizzingAmong({Thing(1), Thing(9)}));
	EXPECT_EQ(order.SegmentOf(Thing(7)), 0u);
	EXPECT_EQ(order.RankOf(Thing(9)), std::nullopt);
	EXPECT_EQ(order.SegmentOf(Thing(2)), 1u);
}

TEST(FizzDrawOrder, EachSegmentComesAfterThePassesOfItsCreatureAndBeforeTheNextCreature)
{
	// The sorted view draws the greatest depth first
	for (uint32_t rank = 0; rank < 4; ++rank)
	{
		const auto creature = CreatureDepth(rank);
		const auto segment = SegmentDepth(rank + 1);
		// The depth pass, the body pass and the eight passes of the eyes come before the segment
		EXPECT_GT(creature - 9, segment);
		EXPECT_GT(segment, CreatureDepth(rank + 1));
	}
	// The draws that come straight after the land keep the greatest depth
	EXPECT_LT(CreatureDepth(0), 0xFFFFFFFFu);
	// All come before the blended things, whose depths never reach the top bit
	EXPECT_GE(SegmentDepth(k_MaxRanked), 0x80000000u);
}

TEST(FizzDrawOrder, TheInstancesOfAMeshAreDrawnSegmentBySegment)
{
	// Three instances in segment 0, none in segment 1, two in segment 2
	std::vector<uint32_t> starts;
	const std::array segments {0u, 0u, 0u, 2u, 2u};
	for (uint32_t filled = 0; filled < segments.size(); ++filled)
	{
		NoteSegment(starts, filled, segments.at(filled));
	}
	EXPECT_EQ(starts, (std::vector<uint32_t> {3, 3}));

	std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> draws;
	ForEachSegment(10, 5, starts,
	               [&draws](uint32_t first, uint32_t count, uint32_t segment) { draws.emplace_back(first, count, segment); });
	const std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> expected {{10, 3, 0}, {13, 2, 2}};
	EXPECT_EQ(draws, expected);
}

TEST(FizzDrawOrder, AMeshWithNoSegmentsIsOneDraw)
{
	std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> draws;
	ForEachSegment(4, 6, {},
	               [&draws](uint32_t first, uint32_t count, uint32_t segment) { draws.emplace_back(first, count, segment); });
	const std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> expected {{4, 6, 0}};
	EXPECT_EQ(draws, expected);
	draws.clear();
	ForEachSegment(4, 0, {},
	               [&draws](uint32_t first, uint32_t count, uint32_t segment) { draws.emplace_back(first, count, segment); });
	EXPECT_TRUE(draws.empty());
}

TEST(FizzDrawOrder, AllInstancesInALaterSegmentAreOneDrawInThatSegment)
{
	std::vector<uint32_t> starts;
	NoteSegment(starts, 0, 1);
	NoteSegment(starts, 1, 1);
	EXPECT_EQ(starts, (std::vector<uint32_t> {0}));
	std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> draws;
	ForEachSegment(0, 2, starts,
	               [&draws](uint32_t first, uint32_t count, uint32_t segment) { draws.emplace_back(first, count, segment); });
	const std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> expected {{0, 2, 1}};
	EXPECT_EQ(draws, expected);
}
