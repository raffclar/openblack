/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/Components/Tree.h"
#include "ECS/MarkedTrees.h"
#include "ECS/Registry.h"

using openblack::ecs::MarkedTrees;
using openblack::ecs::Registry;
namespace components = openblack::ecs::components;

TEST(MarkedTrees, FindsTheTreesThatAreThereAtTheFirstCall)
{
	Registry registry;
	const auto tree = registry.Create();
	registry.Assign<components::Tree>(tree);
	const auto rock = registry.Create();

	const auto& trees = MarkedTrees(registry);
	EXPECT_TRUE(trees.contains(tree));
	EXPECT_FALSE(trees.contains(rock));
	EXPECT_EQ(trees.size(), 1);
}

TEST(MarkedTrees, AddsTheTreesThatAreMadeOrReplaced)
{
	Registry registry;
	auto& trees = MarkedTrees(registry);
	const auto tree = registry.Create();
	registry.Assign<components::Tree>(tree, components::Tree {.bendSlot = 3});
	EXPECT_TRUE(trees.contains(tree));

	// The reader removes a tree that it saw with no slot. A replaced tree can come back with a slot, as after a load.
	trees.remove(tree);
	registry.AssignOrReplace<components::Tree>(tree, components::Tree {.bendSlot = 4});
	EXPECT_TRUE(trees.contains(tree));
}

TEST(MarkedTrees, ForgetsTheTreesThatGo)
{
	Registry registry;
	const auto& trees = MarkedTrees(registry);
	const auto tree = registry.Create();
	registry.Assign<components::Tree>(tree);
	const auto stump = registry.Create();
	registry.Assign<components::Tree>(stump);
	ASSERT_EQ(trees.size(), 2);

	registry.Destroy(tree);
	registry.Remove<components::Tree>(stump);
	EXPECT_TRUE(trees.empty());
}

TEST(MarkedTrees, StaysWithTheRegistryWhenItIsEmptied)
{
	Registry registry;
	const auto& trees = MarkedTrees(registry);
	const auto tree = registry.Create();
	registry.Assign<components::Tree>(tree);
	registry.Reset();
	EXPECT_TRUE(trees.empty());

	const auto again = registry.Create();
	registry.Assign<components::Tree>(again);
	EXPECT_TRUE(MarkedTrees(registry).contains(again));
	EXPECT_EQ(&MarkedTrees(registry), &trees);
}
