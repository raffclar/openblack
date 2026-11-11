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
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureTreeView.h"

using namespace openblack;
using namespace openblack::creature_tree_view;
using creature_tree::Attribute;
using creature_tree::Belief;
using creature_tree::Episode;
using creature_tree::Node;
using creature_tree::Tree;

namespace
{
/// A small tree made by hand:
///   0 Allegiance?
///     0 -> 1 Type?
///            6 -> 3 leaf +0.8 (2)
///            7 -> 4 leaf -0.4 (1)
///     1 -> 2 leaf -1.0 (3)
Tree SmallTree()
{
	Tree tree;
	tree.nodes = {
	    {.test = Attribute::Allegiance, .children = {{0, 1}, {1, 2}}, .branches = 3, .examples = 6},
	    {.test = Attribute::Type, .children = {{6, 3}, {7, 4}}, .branches = 24, .examples = 3},
	    {.bucket = 0, .examples = 3},
	    {.bucket = 9, .examples = 2},
	    {.bucket = 3, .examples = 1},
	};
	return tree;
}

Belief Thing(uint32_t allegiance, std::optional<uint32_t> type)
{
	Belief belief;
	belief.Set(Attribute::Allegiance, allegiance);
	if (type.has_value())
	{
		belief.Set(Attribute::Type, *type);
	}
	return belief;
}
} // namespace

TEST(CreatureTreeView, LaysOutLeavesInColumnsAndParentsOverTheirChildren)
{
	const auto layout = LayOut(SmallTree());
	ASSERT_EQ(layout.nodes.size(), 5u);
	EXPECT_EQ(layout.columns, 3u);
	EXPECT_EQ(layout.rows, 3u);
	// The leaves from the left: the two below the type test, then the enemy leaf
	EXPECT_EQ(layout.nodes[3], (Placed {.x = 0.0f, .y = 2.0f}));
	EXPECT_EQ(layout.nodes[4], (Placed {.x = 1.0f, .y = 2.0f}));
	EXPECT_EQ(layout.nodes[2], (Placed {.x = 2.0f, .y = 1.0f}));
	// Each inner node is half way between its first and its last child
	EXPECT_EQ(layout.nodes[1], (Placed {.x = 0.5f, .y = 1.0f}));
	EXPECT_EQ(layout.nodes[0], (Placed {.x = 1.25f, .y = 0.0f}));
}

TEST(CreatureTreeView, LaysOutAnEmptyTreeAndASingleLeaf)
{
	const auto empty = LayOut(Tree {});
	EXPECT_TRUE(empty.nodes.empty());
	EXPECT_EQ(empty.columns, 0u);
	EXPECT_EQ(empty.rows, 0u);

	Tree leaf;
	leaf.nodes = {{.bucket = 5, .examples = 1}};
	const auto one = LayOut(leaf);
	EXPECT_EQ(one.columns, 1u);
	EXPECT_EQ(one.rows, 1u);
	EXPECT_EQ(one.nodes[0], (Placed {.x = 0.0f, .y = 0.0f}));
}

TEST(CreatureTreeView, LaysOutAGrownTree)
{
	// Friendly villagers are good, enemy villagers bad: the tree tests the allegiance once, with two leaves
	std::vector<Episode> episodes;
	for (const auto [allegiance, feedback] : {std::pair {0u, 1.0f}, std::pair {1u, -1.0f}, std::pair {0u, 1.0f}})
	{
		episodes.push_back({.belief = Thing(allegiance, creature_tree::belief_types::k_Villager), .feedback = feedback});
	}
	const std::vector allowed {Attribute::Allegiance, Attribute::Type};
	const auto tree = creature_tree::Build(episodes, allowed);
	const auto layout = LayOut(tree);
	EXPECT_EQ(layout.columns, 2u);
	EXPECT_EQ(layout.rows, 2u);
	EXPECT_FLOAT_EQ(layout.nodes[0].x, 0.5f);
}

TEST(CreatureTreeView, ColoursLeavesFromRedThroughGreyToGreen)
{
	EXPECT_EQ(LeafColour(-1.0f), k_WorstColour);
	EXPECT_EQ(LeafColour(0.0f), k_NeutralColour);
	EXPECT_EQ(LeafColour(1.0f), k_BestColour);
	// Half way mixes half of each
	EXPECT_EQ(LeafColour(0.5f), (Colour {.r = 80, .g = 150, .b = 95}));
	EXPECT_EQ(LeafColour(-0.5f), (Colour {.r = 160, .g = 80, .b = 80}));
	// Out of range takes the nearest end
	EXPECT_EQ(LeafColour(3.0f), k_BestColour);
	EXPECT_EQ(LeafColour(-3.0f), k_WorstColour);
}

TEST(CreatureTreeView, LeafValueIsTheStepOfTheFeedbackScale)
{
	EXPECT_FLOAT_EQ(LeafValue({.bucket = 0}), -1.0f);
	EXPECT_FLOAT_EQ(LeafValue({.bucket = 9}), 0.8f);
	EXPECT_FLOAT_EQ(LeafValue({.bucket = 10}), 1.0f);
}

TEST(CreatureTreeView, FollowsAThingFromTheRootToItsLeaf)
{
	const auto tree = SmallTree();
	const auto friendlyAnimal = PathOf(tree, Thing(0, creature_tree::belief_types::k_Animal));
	EXPECT_EQ(friendlyAnimal.nodes, (std::vector<size_t> {0, 1, 4}));
	EXPECT_TRUE(friendlyAnimal.reachesLeaf);
	EXPECT_EQ(friendlyAnimal.nodes.back(), creature_tree::LeafOf(tree, Thing(0, creature_tree::belief_types::k_Animal))->node);

	const auto enemy = PathOf(tree, Thing(1, std::nullopt));
	EXPECT_EQ(enemy.nodes, (std::vector<size_t> {0, 2}));
	EXPECT_TRUE(enemy.reachesLeaf);
}

TEST(CreatureTreeView, APathStopsWhereTheTreeHasNoBranchForTheThing)
{
	const auto tree = SmallTree();
	// No type: the walk stops at the type test
	const auto noType = PathOf(tree, Thing(0, std::nullopt));
	EXPECT_EQ(noType.nodes, (std::vector<size_t> {0, 1}));
	EXPECT_FALSE(noType.reachesLeaf);
	// A kind out of the range of kinds has no branch at the type test
	const auto beyond = PathOf(tree, Thing(0, 24));
	EXPECT_EQ(beyond.nodes, (std::vector<size_t> {0, 1}));
	EXPECT_FALSE(beyond.reachesLeaf);
	EXPECT_FALSE(creature_tree::LeafOf(tree, Thing(0, 24)).has_value());
	// An empty tree has no path
	EXPECT_TRUE(PathOf(Tree {}, Thing(0, std::nullopt)).nodes.empty());
}

TEST(CreatureTreeView, MarksTheNodesThatChangedWhenATreeGrewAgain)
{
	const auto before = SmallTree();
	EXPECT_TRUE(SameTree(before, SmallTree()));
	EXPECT_EQ(ChangedNodes(before, before), std::vector<bool>(5, false));

	// The animal leaf took one more example and moved up the scale
	auto after = SmallTree();
	after.nodes[4] = {.bucket = 6, .examples = 2};
	after.nodes[1].examples = 4;
	after.nodes[0].examples = 7;
	EXPECT_FALSE(SameTree(before, after));
	EXPECT_EQ(ChangedNodes(before, after), (std::vector<bool> {true, true, false, false, true}));

	// The enemy leaf became a test of the type, with two new leaves
	auto split = SmallTree();
	split.nodes[2] = {.test = Attribute::Type, .children = {{6, 5}, {7, 6}}, .examples = 3};
	split.nodes.push_back({.bucket = 0, .examples = 2});
	split.nodes.push_back({.bucket = 2, .examples = 1});
	EXPECT_EQ(ChangedNodes(before, split), (std::vector<bool> {false, false, true, false, false, true, true}));

	// Every node of a tree that grew from nothing is new
	EXPECT_EQ(ChangedNodes(Tree {}, before), std::vector<bool>(5, true));
	EXPECT_TRUE(ChangedNodes(before, Tree {}).empty());
}

TEST(CreatureTreeView, ANewBranchValueIsANewNode)
{
	const auto before = SmallTree();
	auto after = SmallTree();
	after.nodes[1].children.emplace_back(8, 5);
	after.nodes.push_back({.bucket = 10, .examples = 1});
	EXPECT_EQ(ChangedNodes(before, after), (std::vector<bool> {false, false, false, false, false, true}));
}

TEST(CreatureTreeView, ExamplesKeepTheTurnTheViewFirstSawThem)
{
	const Episode a {.belief = Thing(0, 6), .feedback = 0.5f};
	const Episode b {.belief = Thing(1, 6), .feedback = -0.5f};
	const Episode c {.belief = Thing(1, 7), .feedback = 1.0f};

	// At the first look the view does not know when the examples came
	const std::vector first {a, b};
	const auto unknown = SeenTurns({}, {}, first, 10, true);
	EXPECT_EQ(unknown, (std::vector<std::optional<uint32_t>> {std::nullopt, std::nullopt}));

	// A new example at the end gets the turn now
	const std::vector added {a, b, c};
	const auto turns = SeenTurns(first, unknown, added, 12, false);
	EXPECT_EQ(turns, (std::vector<std::optional<uint32_t>> {std::nullopt, std::nullopt, 12u}));

	// A full list puts the newest in place of the last one
	const std::vector replaced {a, b, a};
	EXPECT_EQ(SeenTurns(added, turns, replaced, 15, false),
	          (std::vector<std::optional<uint32_t>> {std::nullopt, std::nullopt, 15u}));
}

TEST(CreatureTreeView, NamesValuesAndNodes)
{
	EXPECT_EQ(ValueName(Attribute::Allegiance, 1), "1 enemy");
	EXPECT_EQ(ValueName(Attribute::Type, creature_tree::belief_types::k_Villager), "6 villager");
	EXPECT_EQ(ValueName(Attribute::PlayerNumber, creature_tree::k_NoPlayerNumber), "7 no player");
	EXPECT_EQ(ValueName(Attribute::Height, 2), "2");
	EXPECT_EQ(NodeLabel(SmallTree().nodes[0]), creature_tree::Name(Attribute::Allegiance));
	EXPECT_EQ(NodeLabel(SmallTree().nodes[3]), "+0.8 (2)");
}

TEST(CreatureTreeView, APathEndsOnABranchWithNoExamplesInTheRange)
{
	auto tree = SmallTree();
	// The type test has a branch for each of the 24 kinds; only villagers and animals have examples
	tree.nodes[1].bucket = 7;
	const auto town = PathOf(tree, Thing(0, creature_tree::belief_types::k_Town));
	EXPECT_EQ(town.nodes, (std::vector<size_t> {0, 1}));
	EXPECT_TRUE(town.reachesLeaf);
	EXPECT_EQ(town.emptyBranch, creature_tree::belief_types::k_Town);
	EXPECT_EQ(PathEnd(town), " (empty (mean of the node above))");
	// It takes the value of the node above, as the tree does
	EXPECT_FLOAT_EQ(creature_tree::Evaluate(tree, Thing(0, creature_tree::belief_types::k_Town)), LeafValue(tree.nodes[1]));
	// A kind out of the range has no branch
	const auto beyond = PathOf(tree, Thing(0, 24));
	EXPECT_FALSE(beyond.reachesLeaf);
	EXPECT_FALSE(beyond.emptyBranch.has_value());
	EXPECT_EQ(PathEnd(beyond), " (no leaf: no branch for it)");
	EXPECT_EQ(PathEnd(PathOf(tree, Thing(1, std::nullopt))), "");
}
