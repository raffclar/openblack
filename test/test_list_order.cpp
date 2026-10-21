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
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/ListPlace.h"
#include "ECS/ListOrder.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{

/// A thing of a listed kind, for the tests
struct Listed
{
	int value {0};
};

} // namespace

// The things of a kind take their turns newest first, also after one of them leaves the list
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ListOrder, TheNewestFirstAlsoAfterOneLeaves)
{
	Registry registry;
	std::vector<entt::entity> made;
	for (int i = 0; i < 5; ++i)
	{
		const auto entity = registry.Create();
		registry.Assign<Listed>(entity, Listed {.value = i});
		list_order::Join(registry, entity);
		made.push_back(entity);
	}
	EXPECT_EQ(list_order::Walk<Listed>(registry), (std::vector {made[4], made[3], made[2], made[1], made[0]}));

	// The removal moves the last thing of the store into the gap, but the list keeps its order
	registry.Destroy(made[1]);
	const auto newest = registry.Create();
	registry.Assign<Listed>(newest);
	list_order::Join(registry, newest);
	EXPECT_EQ(list_order::Walk<Listed>(registry), (std::vector {newest, made[4], made[3], made[2], made[0]}));
	EXPECT_EQ(registry.Context().listedMade, 6u);
}

// A thing with no place, as from an old save, comes after the things with a place, the larger id first
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ListOrder, ThingsWithNoPlaceComeLastByTheirIds)
{
	std::vector<list_order::Entry> list {
	    {.id = entt::entity {3}, .made = 0}, {.id = entt::entity {9}, .made = 0}, {.id = entt::entity {1}, .made = 4}};
	list_order::NewestFirst(list);
	EXPECT_EQ(list[0].id, entt::entity {1});
	EXPECT_EQ(list[1].id, entt::entity {9});
	EXPECT_EQ(list[2].id, entt::entity {3});
}

// One thing a turn, from the newest to the oldest, then the newest again
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ListOrder, OneATurnFromTheNewestToTheOldest)
{
	const std::vector<uint32_t> oldestFirst {10, 11, 12};
	const std::span<const uint32_t> list(oldestFirst);
	EXPECT_EQ(list_order::NextInTurn(list, std::optional<uint32_t> {}), 2u);
	EXPECT_EQ(list_order::NextInTurn(list, std::optional<uint32_t> {12}), 1u);
	EXPECT_EQ(list_order::NextInTurn(list, std::optional<uint32_t> {11}), 0u);
	EXPECT_EQ(list_order::NextInTurn(list, std::optional<uint32_t> {10}), 2u);
	// A cursor that is not in the list starts again at the newest
	EXPECT_EQ(list_order::NextInTurn(list, std::optional<uint32_t> {99}), 2u);
	EXPECT_FALSE(list_order::NextInTurn(std::span<const uint32_t> {}, std::optional<uint32_t> {}).has_value());
}

// When the thing that had the last turn leaves, the cursor moves to the next older thing. Thus the next turn goes to
// the thing after that one.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ListOrder, TheThingAfterOneThatLeavesLosesItsTurn)
{
	const std::vector<uint32_t> before {10, 11, 12};
	const auto cursor = list_order::CursorAfterLeave(std::span<const uint32_t>(before), std::optional<uint32_t> {12}, 12u);
	EXPECT_EQ(cursor, 11u);
	const std::vector<uint32_t> after {10, 11};
	EXPECT_EQ(list_order::NextInTurn(std::span<const uint32_t>(after), cursor), 0u);
	// Another thing leaves: the cursor stays
	EXPECT_EQ(list_order::CursorAfterLeave(std::span<const uint32_t>(before), std::optional<uint32_t> {12}, 10u), 12u);
	// The oldest leaves while it is the cursor: no cursor, and the next turn goes to the newest
	EXPECT_FALSE(
	    list_order::CursorAfterLeave(std::span<const uint32_t>(before), std::optional<uint32_t> {10}, 10u).has_value());
}
