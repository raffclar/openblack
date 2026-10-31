/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <entt/entity/storage.hpp>
#include <gtest/gtest.h>

#include "ECS/EntityStorage.h"

using namespace openblack;

namespace
{
/// A registry gives the number of a destroyed entity again, with a new version
struct GivenAgain
{
	entt::registry registry;
	entt::entity older {registry.create()};
	entt::entity newer {[this] {
		registry.destroy(older);
		return registry.create();
	}()};
};
} // namespace

TEST(EntityStorage, TheRegistryGivesTheNumberOfADestroyedEntityAgainWithANewVersion)
{
	GivenAgain given;
	ASSERT_EQ(entt::to_entity(given.newer), entt::to_entity(given.older));
	ASSERT_NE(entt::to_version(given.newer), entt::to_version(given.older));

	// The fault: the storage does not hold the new entity, but the row of the older version uses its slot, so an
	// emplace of the new entity fails
	entt::storage<int> storage;
	storage.emplace(given.older, 7);
	EXPECT_FALSE(storage.contains(given.newer));
	EXPECT_NE(storage.current(given.newer), entt::to_version(static_cast<entt::entity>(entt::tombstone)));
}

TEST(EntityStorage, TakeOlderRemovesTheRowOfTheOlderVersionAndGivesItsValue)
{
	GivenAgain given;
	entt::storage<int> storage;
	storage.emplace(given.older, 7);

	const auto taken = ecs::entity_storage::TakeOlder(storage, given.newer);
	ASSERT_TRUE(taken.has_value());
	EXPECT_EQ(*taken, 7);
	EXPECT_FALSE(storage.contains(given.older));
	EXPECT_TRUE(storage.empty());

	// The slot is free, so the new entity can have a row
	storage.emplace(given.newer, 9);
	EXPECT_TRUE(storage.contains(given.newer));
	EXPECT_EQ(storage.get(given.newer), 9);
}

TEST(EntityStorage, TakeOlderKeepsTheRowOfTheSameVersion)
{
	entt::registry registry;
	const auto entity = registry.create();
	entt::storage<int> storage;
	storage.emplace(entity, 3);

	EXPECT_FALSE(ecs::entity_storage::TakeOlder(storage, entity).has_value());
	EXPECT_TRUE(storage.contains(entity));
	EXPECT_EQ(storage.get(entity), 3);
}

TEST(EntityStorage, TakeOlderGivesNoValueWhenTheStorageHoldsNoRowForTheNumber)
{
	entt::registry registry;
	const auto held = registry.create();
	const auto other = registry.create();
	entt::storage<int> storage;
	storage.emplace(held, 3);

	EXPECT_FALSE(ecs::entity_storage::TakeOlder(storage, other).has_value());
	EXPECT_EQ(storage.size(), 1u);

	entt::storage<int> empty;
	EXPECT_FALSE(ecs::entity_storage::TakeOlder(empty, other).has_value());
}

TEST(EntityStorage, TakeOlderKeepsTheRowsOfOtherNumbers)
{
	GivenAgain given;
	const auto other = given.registry.create();
	entt::storage<int> storage;
	storage.emplace(other, 5);
	storage.emplace(given.older, 7);

	EXPECT_EQ(ecs::entity_storage::TakeOlder(storage, given.newer), 7);
	EXPECT_TRUE(storage.contains(other));
	EXPECT_EQ(storage.get(other), 5);
}
