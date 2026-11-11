/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureTreeView.h"

#include <cmath>

#include <algorithm>
#include <utility>

#include <fmt/format.h>

using namespace openblack;
using namespace openblack::creature_tree_view;
using creature_tree::Attribute;
using creature_tree::Node;
using creature_tree::Tree;

namespace
{
using Branch = std::pair<uint32_t, size_t>;

/// Puts a node and the nodes below it. A node that is seen two times stops the walk, so that a broken tree cannot loop.
void Place(const Tree& tree, size_t index, size_t depth, Layout& layout, std::vector<bool>& seen)
{
	if (index >= tree.nodes.size() || seen.at(index))
	{
		return;
	}
	seen.at(index) = true;
	layout.rows = std::max(layout.rows, depth + 1);
	auto& placed = layout.nodes.at(index);
	placed.y = static_cast<float>(depth);
	const auto& node = tree.nodes.at(index);
	std::optional<float> first;
	std::optional<float> last;
	for (const auto& [value, child] : node.children)
	{
		Place(tree, child, depth + 1, layout, seen);
		if (child < tree.nodes.size())
		{
			const auto x = layout.nodes.at(child).x;
			first = first.value_or(x);
			last = x;
		}
	}
	if (first.has_value() && last.has_value())
	{
		layout.nodes.at(index).x = (*first + *last) * 0.5f;
		return;
	}
	layout.nodes.at(index).x = static_cast<float>(layout.columns);
	++layout.columns;
}

void MarkAll(const Tree& tree, size_t index, std::vector<bool>& changed)
{
	if (index >= tree.nodes.size() || changed.at(index))
	{
		return;
	}
	changed.at(index) = true;
	for (const auto& [value, child] : tree.nodes.at(index).children)
	{
		MarkAll(tree, child, changed);
	}
}

void Compare(const Tree& before, size_t old, const Tree& after, size_t now, std::vector<bool>& changed, std::vector<bool>& seen)
{
	if (now >= after.nodes.size() || seen.at(now))
	{
		return;
	}
	seen.at(now) = true;
	const auto& was = before.nodes.at(old);
	const auto& is = after.nodes.at(now);
	if (was.test != is.test)
	{
		MarkAll(after, now, changed);
		return;
	}
	changed.at(now) = was.examples != is.examples || (!is.test.has_value() && was.bucket != is.bucket);
	for (const auto& [value, child] : is.children)
	{
		const auto found = std::ranges::find(was.children, value, &Branch::first);
		if (found == was.children.end() || found->second >= before.nodes.size())
		{
			MarkAll(after, child, changed);
		}
		else
		{
			Compare(before, found->second, after, child, changed, seen);
		}
	}
}
} // namespace

Layout creature_tree_view::LayOut(const Tree& tree)
{
	Layout layout;
	layout.nodes.resize(tree.nodes.size());
	if (tree.nodes.empty())
	{
		return layout;
	}
	std::vector<bool> seen(tree.nodes.size(), false);
	Place(tree, 0, 0, layout, seen);
	return layout;
}

float creature_tree_view::LeafValue(const Node& node)
{
	return creature_tree::k_Buckets.at(std::min(node.bucket, creature_tree::k_Buckets.size() - 1));
}

Colour creature_tree_view::LeafColour(float value)
{
	const float share = std::clamp(value, -1.0f, 1.0f);
	const auto& end = share < 0.0f ? k_WorstColour : k_BestColour;
	const float t = std::abs(share);
	const auto mix = [t](uint8_t from, uint8_t to) {
		return static_cast<uint8_t>(
		    std::lround(static_cast<float>(from) + (static_cast<float>(to) - static_cast<float>(from)) * t));
	};
	return {.r = mix(k_NeutralColour.r, end.r), .g = mix(k_NeutralColour.g, end.g), .b = mix(k_NeutralColour.b, end.b)};
}

Path creature_tree_view::PathOf(const Tree& tree, const creature_tree::Belief& belief)
{
	Path path;
	if (tree.nodes.empty())
	{
		return path;
	}
	size_t index = 0;
	// A tree has no more levels than nodes, so the walk stops also on a broken tree
	while (path.nodes.size() < tree.nodes.size())
	{
		path.nodes.push_back(index);
		const auto& node = tree.nodes.at(index);
		if (!node.test.has_value())
		{
			path.reachesLeaf = true;
			return path;
		}
		const auto value = belief.Value(*node.test);
		if (!value.has_value())
		{
			return path;
		}
		const auto found = std::ranges::find(node.children, *value, &Branch::first);
		if (found == node.children.end() && *value < node.branches)
		{
			path.reachesLeaf = true;
			path.emptyBranch = *value;
			return path;
		}
		if (found == node.children.end() || found->second >= tree.nodes.size())
		{
			return path;
		}
		index = found->second;
	}
	return path;
}

std::string creature_tree_view::PathEnd(const Path& path)
{
	if (path.emptyBranch.has_value())
	{
		return std::string(" (") + k_EmptyBranchLabel + ")";
	}
	return path.reachesLeaf ? "" : " (no leaf: no branch for it)";
}

bool creature_tree_view::SameTree(const Tree& a, const Tree& b)
{
	return std::ranges::equal(a.nodes, b.nodes, [](const Node& x, const Node& y) {
		return x.test == y.test && x.children == y.children && x.branches == y.branches && x.bucket == y.bucket &&
		       x.examples == y.examples;
	});
}

std::vector<bool> creature_tree_view::ChangedNodes(const Tree& before, const Tree& after)
{
	std::vector<bool> changed(after.nodes.size(), false);
	if (after.nodes.empty())
	{
		return changed;
	}
	if (before.nodes.empty())
	{
		MarkAll(after, 0, changed);
		return changed;
	}
	std::vector<bool> seen(after.nodes.size(), false);
	Compare(before, 0, after, 0, changed, seen);
	return changed;
}

bool creature_tree_view::SameEpisode(const creature_tree::Episode& a, const creature_tree::Episode& b)
{
	return a.belief == b.belief && a.feedback == b.feedback;
}

std::vector<std::optional<uint32_t>> creature_tree_view::SeenTurns(std::span<const creature_tree::Episode> before,
                                                                   std::span<const std::optional<uint32_t>> beforeTurns,
                                                                   std::span<const creature_tree::Episode> now, uint32_t turn,
                                                                   bool firstLook)
{
	std::vector<std::optional<uint32_t>> turns(now.size());
	if (firstLook)
	{
		return turns;
	}
	for (size_t i = 0; i < now.size(); ++i)
	{
		const bool kept = i < before.size() && i < beforeTurns.size() && SameEpisode(before[i], now[i]);
		turns.at(i) = kept ? beforeTurns[i] : std::optional(turn);
	}
	return turns;
}

std::string creature_tree_view::ValueName(Attribute attribute, uint32_t value)
{
	const char* name = nullptr;
	switch (attribute)
	{
	case Attribute::Allegiance:
		name = value == creature_tree::allegiance::k_Friendly ? "friendly"
		       : value == creature_tree::allegiance::k_Enemy  ? "enemy"
		                                                      : nullptr;
		break;
	case Attribute::Origin:
		name = value == creature_tree::origin::k_Natural   ? "natural"
		       : value == creature_tree::origin::k_ManMade ? "man-made"
		       : value == creature_tree::origin::k_Magic   ? "magic"
		                                                   : nullptr;
		break;
	case Attribute::PlayerNumber:
		name = value == creature_tree::k_NoPlayerNumber ? "no player" : nullptr;
		break;
	case Attribute::Type:
		name = creature_tree::BeliefName(value);
		break;
	case Attribute::OnFire:
		name = value == 0 ? "on fire" : value == 1 ? "not on fire" : nullptr;
		break;
	case Attribute::AbodeBeingBuilt:
		name = value == 0 ? "being built" : value == 1 ? "built" : nullptr;
		break;
	default:
		break;
	}
	return name != nullptr ? fmt::format("{} {}", value, name) : std::to_string(value);
}

std::string creature_tree_view::NodeLabel(const Node& node)
{
	if (node.test.has_value())
	{
		return creature_tree::Name(*node.test);
	}
	return fmt::format("{:+.1f} ({})", LeafValue(node), node.examples);
}
