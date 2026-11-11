/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Creature/CreatureDecisionTree.h"

/// The maths of the developer view of a creature's decision trees: where each node goes in the drawing, the colour of
/// a leaf, the path of a thing through a tree, and which nodes changed when a tree grew again. The view only reads the
/// trees.
namespace openblack::creature_tree_view
{

/// The place of a node in the drawing. x is in leaf columns: the leaves are 0, 1, 2 and so on from the left, and an
/// inner node is half way between its first and its last child. y is the depth: 0 at the root.
struct Placed
{
	float x {0.0f};
	float y {0.0f};

	bool operator==(const Placed& other) const = default;
};
struct Layout
{
	/// One place for each node of the tree, by the node's index
	std::vector<Placed> nodes;
	/// The count of leaf columns
	size_t columns {0};
	/// The count of rows: the depth of the deepest leaf, plus 1
	size_t rows {0};
};
/// The places of the nodes of a tree, in the order that a walk from the root goes through the branches
[[nodiscard]] Layout LayOut(const creature_tree::Tree& tree);

/// The value of a leaf, from -1 to 1
[[nodiscard]] float LeafValue(const creature_tree::Node& node);

struct Colour
{
	uint8_t r {0};
	uint8_t g {0};
	uint8_t b {0};

	bool operator==(const Colour& other) const = default;
};
constexpr Colour k_WorstColour {.r = 210, .g = 50, .b = 50};
constexpr Colour k_NeutralColour {.r = 110, .g = 110, .b = 110};
constexpr Colour k_BestColour {.r = 50, .g = 190, .b = 80};
/// The colour of a leaf value: red at -1, grey at 0, green at 1, and mixed between them. A value out of range gets the
/// colour of the nearest end.
[[nodiscard]] Colour LeafColour(float value);

/// The nodes that a thing goes through, from the root
struct Path
{
	std::vector<size_t> nodes;
	/// The path ends at a leaf. If not, the thing does not have the attribute of the last node, or has a value that no
	/// branch of the last node has.
	bool reachesLeaf {false};
};
[[nodiscard]] Path PathOf(const creature_tree::Tree& tree, const creature_tree::Belief& belief);

/// Two trees are the same when each node tests the same attribute, has the same branches, the same step of the
/// feedback scale and the same count of examples
[[nodiscard]] bool SameTree(const creature_tree::Tree& a, const creature_tree::Tree& b);
/// For each node of the new tree, whether it changed from the old tree. The two trees are compared from the root, by
/// the values of the branches. A node changes when the old tree has no node at its place, or the node tests another
/// attribute, or it has another step of the feedback scale or another count of examples. All the nodes below a node
/// that is new are new too.
[[nodiscard]] std::vector<bool> ChangedNodes(const creature_tree::Tree& before, const creature_tree::Tree& after);

/// Two examples are the same when the thing and the feedback are the same
[[nodiscard]] bool SameEpisode(const creature_tree::Episode& a, const creature_tree::Episode& b);
/// The turn at which the view first saw each example. An example at the same place as before, and the same as before,
/// keeps its turn. Another example gets the turn now. Before the first look, give no examples before: then each
/// example gets no turn, because the view does not know when it came.
[[nodiscard]] std::vector<std::optional<uint32_t>> SeenTurns(std::span<const creature_tree::Episode> before,
                                                             std::span<const std::optional<uint32_t>> beforeTurns,
                                                             std::span<const creature_tree::Episode> now, uint32_t turn,
                                                             bool firstLook);

/// A value of an attribute in words where the value has a name (an enemy, a villager), else the number
[[nodiscard]] std::string ValueName(creature_tree::Attribute attribute, uint32_t value);
/// The text of a node: the attribute that an inner node tests, or the value of a leaf and its count of examples
[[nodiscard]] std::string NodeLabel(const creature_tree::Node& node);

} // namespace openblack::creature_tree_view
