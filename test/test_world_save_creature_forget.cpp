/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <entt/entity/registry.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/CreatureFriends.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/WorldSave.h"

using namespace openblack;
using openblack::ecs::components::CreatureFriends;
using openblack::ecs::components::CreatureMindState;

namespace
{

/// A creature with a friend that a script made and a plan put aside
entt::entity CreatureWithFriendAndKeptPlan(entt::registry& registry, uint32_t friendNumber)
{
	const auto creature = registry.create();
	registry.emplace<CreatureFriends>(creature, CreatureFriends {.friends = {friendNumber}});
	auto& mind = registry.emplace<CreatureMindState>(creature);
	mind.keptPlan = creature_planner::Plan {};
	return creature;
}

} // namespace

// The game saves no friends that a script made and no plan put aside, so a load of a saved game forgets both
TEST(WorldSaveCreatureForget, ALoadForgetsTheFriendsAndThePlanPutAside)
{
	entt::registry registry;
	const auto first = CreatureWithFriendAndKeptPlan(registry, 2);
	const auto second = CreatureWithFriendAndKeptPlan(registry, entt::to_integral(first));
	ecs::world_save::ForgetUnsavedState(registry);
	for (const auto creature : {first, second})
	{
		EXPECT_FALSE(registry.all_of<CreatureFriends>(creature));
		EXPECT_FALSE(registry.get<CreatureMindState>(creature).keptPlan.has_value());
	}
}

// The load of a save that keeps only the creatures forgets the two values of each creature that it puts back
TEST(WorldSaveCreatureForget, OneCreatureForgetsOnlyItsOwnValues)
{
	entt::registry registry;
	const auto first = CreatureWithFriendAndKeptPlan(registry, 2);
	const auto second = CreatureWithFriendAndKeptPlan(registry, entt::to_integral(first));
	ecs::world_save::ForgetUnsavedCreatureState(registry, first);
	EXPECT_FALSE(registry.all_of<CreatureFriends>(first));
	EXPECT_FALSE(registry.get<CreatureMindState>(first).keptPlan.has_value());
	EXPECT_TRUE(registry.all_of<CreatureFriends>(second));
	EXPECT_TRUE(registry.get<CreatureMindState>(second).keptPlan.has_value());
}

// A creature with no mind and no friends is left as it is
TEST(WorldSaveCreatureForget, ACreatureWithNeitherValueIsNotChanged)
{
	entt::registry registry;
	const auto creature = registry.create();
	ecs::world_save::ForgetUnsavedCreatureState(registry, creature);
	EXPECT_TRUE(registry.valid(creature));
	EXPECT_FALSE(registry.all_of<CreatureFriends>(creature));
}
