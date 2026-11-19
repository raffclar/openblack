/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"
#include "ECS/UnfiledTrees.h"

using openblack::ecs::Registry;
using openblack::ecs::UnfiledTrees;
namespace components = openblack::ecs::components;

namespace
{

entt::entity TreeIn(Registry& registry, std::vector<uint32_t> cells)
{
	const auto tree = registry.Create();
	registry.Assign<components::Tree>(tree);
	registry.Assign<components::MapCellResident>(tree, components::MapCellResident {.cells = std::move(cells)});
	return tree;
}

} // namespace

TEST(UnfiledTrees, FindsTheTreesThatAreThereAtTheFirstCall)
{
	Registry registry;
	const auto filed = TreeIn(registry, {7});
	const auto unfiled = registry.Create();
	registry.Assign<components::Tree>(unfiled);
	const auto rock = registry.Create();
	registry.Assign<components::MapCellResident>(rock);

	const auto& trees = UnfiledTrees(registry);
	EXPECT_FALSE(trees.contains(filed));
	EXPECT_TRUE(trees.contains(unfiled));
	EXPECT_FALSE(trees.contains(rock));
	EXPECT_EQ(trees.size(), 1);
}

TEST(UnfiledTrees, FollowsTheTreesAsTheyGoIntoAndOutOfTheCells)
{
	Registry registry;
	const auto& trees = UnfiledTrees(registry);

	// A new tree is not in the cells until the map files it
	const auto tree = registry.Create();
	registry.Assign<components::Tree>(tree);
	EXPECT_TRUE(trees.contains(tree));
	registry.AssignOrReplace<components::MapCellResident>(tree, components::MapCellResident {.cells = {3, 4}});
	EXPECT_FALSE(trees.contains(tree));

	// Filed again with no cell, as off the map
	registry.AssignOrReplace<components::MapCellResident>(tree, components::MapCellResident {});
	EXPECT_TRUE(trees.contains(tree));
	registry.AssignOrReplace<components::MapCellResident>(tree, components::MapCellResident {.cells = {5}});
	EXPECT_FALSE(trees.contains(tree));

	// Taken out of the cells, as the hand picks it up
	registry.Remove<components::MapCellResident>(tree);
	EXPECT_TRUE(trees.contains(tree));
}

TEST(UnfiledTrees, ForgetsTheTreesThatGo)
{
	Registry registry;
	const auto& trees = UnfiledTrees(registry);
	const auto tree = registry.Create();
	registry.Assign<components::Tree>(tree);
	const auto filed = TreeIn(registry, {1});
	ASSERT_TRUE(trees.contains(tree));

	registry.Destroy(tree);
	registry.Destroy(filed);
	EXPECT_TRUE(trees.empty());

	// A thing that is no longer a tree is not in the set, also when it is not in the cells
	const auto stump = TreeIn(registry, {});
	ASSERT_TRUE(trees.contains(stump));
	registry.Remove<components::Tree>(stump);
	EXPECT_FALSE(trees.contains(stump));
	registry.Remove<components::MapCellResident>(stump);
	EXPECT_FALSE(trees.contains(stump));
}

TEST(UnfiledTrees, StaysWithTheRegistryWhenItIsEmptied)
{
	Registry registry;
	const auto& trees = UnfiledTrees(registry);
	const auto tree = registry.Create();
	registry.Assign<components::Tree>(tree);
	registry.Reset();
	EXPECT_TRUE(trees.empty());

	const auto again = registry.Create();
	registry.Assign<components::Tree>(again);
	EXPECT_TRUE(UnfiledTrees(registry).contains(again));
	EXPECT_EQ(&UnfiledTrees(registry), &trees);
}
