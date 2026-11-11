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

#include <array>
#include <optional>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <imgui.h>

#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureMindModel.h"

namespace openblack::debug::gui
{

/// The part of the Creature Mind window that shows a creature's decision trees: a list of the desires, and for the
/// chosen desire each of its two trees as a node graph and as a text tree, with the examples that the tree grew from.
/// It shows the path of the planner's thing, and of a thing that the developer picks, through the tree, and marks the
/// nodes that changed when the tree last grew again. It only reads the mind.
class CreatureMindTrees
{
public:
	/// Looks at the trees of the creature, to find the trees that grew again since the last look. The window calls this
	/// only while it is open.
	void Observe(entt::entity creature, uint32_t turn) noexcept;
	void Draw(entt::entity creature) noexcept;

private:
	using TreeKind = creature_mind_model::TreeKind;

	/// What the view saw of one tree at its last look
	struct Watch
	{
		creature_tree::Tree tree;
		std::vector<creature_tree::Episode> episodes;
		/// The turn at which the view first saw each example; none for an example that was there at the first look
		std::vector<std::optional<uint32_t>> seen;
		/// The turn at which the tree last grew again while the view looked, and the nodes that changed then
		std::optional<uint32_t> grownTurn;
		std::vector<bool> changed;
		/// The time of the interface clock when the tree grew, for the blink of the changed nodes
		double grownAt {0.0};
	};
	/// How the graph of one tree is moved and scaled
	struct GraphView
	{
		ImVec2 pan {0.0f, 0.0f};
		float zoom {1.0f};
		/// The graph fits itself in its area at the next draw
		bool fit {true};
	};

	void DrawDesireList(entt::entity creature) noexcept;
	void DrawTree(entt::entity creature, TreeKind kind) noexcept;
	void DrawThingPicker() noexcept;
	/// The paths through a tree: of the thing of the planner's best plan, and of the thing that the developer picked
	struct Paths
	{
		std::vector<size_t> planner;
		std::vector<size_t> picked;
	};
	void DrawGraph(const creature_tree::Tree& tree, const Watch& watch, const Paths& paths, GraphView& view) noexcept;
	void DrawTextTree(const creature_tree::Tree& tree, const Watch& watch, const Paths& paths, size_t index,
	                  std::optional<uint32_t> branch, size_t depth) noexcept;
	void DrawExamples(const Watch& watch) noexcept;
	[[nodiscard]] Watch& WatchOf(TreeKind kind, creature_desires::Desire desire) noexcept;

	std::optional<entt::entity> _observed;
	/// The view looked at the trees of the observed creature before
	bool _primed {false};
	std::array<std::array<Watch, creature_desires::k_DesireCount>, creature_mind_model::k_TreeKinds> _watches {};
	std::array<std::array<GraphView, creature_desires::k_DesireCount>, creature_mind_model::k_TreeKinds> _views {};
	std::optional<creature_desires::Desire> _desire;
	/// The desire whose trees the view showed at the last draw
	std::optional<creature_desires::Desire> _shownDesire;
	/// The thing that the developer picked, to show its path through the trees
	std::optional<entt::entity> _thing;
	bool _thingFollowsHand {false};
	int _thingId {0};
};

} // namespace openblack::debug::gui
