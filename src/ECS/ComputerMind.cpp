/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerMind.h"

#include <cstdint>

#include <algorithm>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

using namespace openblack::computer_mind;

namespace
{

/// Adds the node of the kind and the nodes below it, in the order that the game makes them: a node, then the whole
/// branch of each child in turn
uint16_t AddBranch(std::vector<Node>& nodes, NodeKind kind, std::optional<uint16_t> parent)
{
	const auto& info = InfoOf(kind);
	const auto index = static_cast<uint16_t>(nodes.size());
	nodes.push_back({.kind = kind, .parent = parent, .weight = info.weight});
	for (uint8_t i = 0; i < info.childCount; ++i)
	{
		const auto child = AddBranch(nodes, info.children.at(i), index);
		auto& node = nodes[index];
		node.children.at(node.childCount) = child;
		++node.childCount;
	}
	return index;
}

/// The suppression of the node comes back toward 1 for the turns since the node was last looked at
void RecoverSuppression(Node& node, uint32_t turn)
{
	if (node.suppression == 1.0f)
	{
		return;
	}
	const auto turns = static_cast<float>(static_cast<uint64_t>(turn - node.lastWorkedOut));
	const float rate = InfoOf(node.kind).recovery;
	const float recovered = ((rate - 1.0f) * turns + 1.0f) * node.suppression;
	node.suppression = recovered < 1.0f ? recovered : 1.0f;
}

/// The first object that a node above has, none when no node above has one
entt::entity ObjectAbove(const Mind& mind, uint16_t node)
{
	for (auto above = mind.nodes[node].parent; above.has_value(); above = mind.nodes[*above].parent)
	{
		if (mind.nodes[*above].object != entt::null)
		{
			return mind.nodes[*above].object;
		}
	}
	return entt::null;
}

/// Looks at the children of the node. A child is not looked at when the node cannot get stronger with it.
void WorkOutChildren(Mind& mind, World& world, uint16_t node, uint32_t turn)
{
	if (mind.nodes[node].childCount == 0)
	{
		return;
	}
	float best = 0.0f;
	for (uint8_t i = 0; i < mind.nodes[node].childCount; ++i)
	{
		const auto child = mind.nodes[node].children.at(i);
		const float most = mind.nodes[node].weight * mind.nodes[node].suppression;
		if (most < best)
		{
			continue;
		}
		const float childValue = WorkOut(mind, world, child, turn);
		const float value = childValue * mind.nodes[node].weight * mind.nodes[node].suppression;
		if (value > best)
		{
			best = value;
			mind.nodes[node].bestLeaf = mind.nodes[child].bestLeaf;
		}
	}
	auto& self = mind.nodes[node];
	self.value = InfoOf(self.kind).intensity ? best * self.value : best;
}

/// Starts the action of the node, or of the first node above that has an action
bool PerformNode(Mind& mind, World& world, uint16_t node, uint32_t turn)
{
	for (std::optional<uint16_t> at = node; at.has_value(); at = mind.nodes[*at].parent)
	{
		auto& current = mind.nodes[*at];
		current.lastPerformed = turn;
		const auto& info = InfoOf(current.kind);
		if (!info.perform)
		{
			continue;
		}
		world.ClearAction();
		if (world.Perform(mind, *at))
		{
			mind.nodes[*at].suppression = info.afterPerform;
			world.StartAction();
			return true;
		}
		mind.nodes[*at].suppression = k_FailedSuppression;
		return false;
	}
	return false;
}

/// Calls the function for each node that the name finds, depth first from the root
void ForEachNamed(const Mind& mind, std::string_view name, const std::function<void(uint16_t)>& each)
{
	std::vector<uint16_t> stack {0};
	while (!stack.empty())
	{
		const auto index = stack.back();
		stack.pop_back();
		const auto& node = mind.nodes[index];
		// A node name ends at its last colon. A longer name that starts with the node name also finds the node.
		auto own = InfoOf(node.kind).name;
		if (name.size() >= own.size() && name.substr(0, own.size()) == own)
		{
			each(index);
		}
		for (auto i = node.childCount; i > 0; --i)
		{
			stack.push_back(node.children.at(i - 1));
		}
	}
}

} // namespace

Mind openblack::computer_mind::MakeMind()
{
	Mind mind;
	AddBranch(mind.nodes, NodeKind::Root, std::nullopt);
	return mind;
}

float openblack::computer_mind::WorkOut(Mind& mind, World& world, uint16_t node, uint32_t turn)
{
	{
		auto& self = mind.nodes[node];
		RecoverSuppression(self, turn);
		self.bestLeaf.reset();
		self.object = entt::null;
		self.lastWorkedOut = turn;
		self.value = 0.0f;
	}
	const auto& info = InfoOf(mind.nodes[node].kind);
	if (info.intensity)
	{
		const float intensity = world.Intensity(mind, node);
		auto& self = mind.nodes[node];
		self.bestLeaf = node;
		self.value = (intensity + self.value) * self.weight * self.suppression;
		// A node that is not wanted does not look at its children
		if (self.value == 0.0f)
		{
			return self.value;
		}
	}
	else if (mind.nodes[node].parent.has_value())
	{
		mind.nodes[node].object = ObjectAbove(mind, node);
	}
	WorkOutChildren(mind, world, node, turn);
	return mind.nodes[node].value;
}

float openblack::computer_mind::WorkOutTopLayer(Mind& mind, World& world, uint32_t turn)
{
	float best = 0.0f;
	for (uint8_t i = 0; i < mind.nodes[0].childCount; ++i)
	{
		const auto top = mind.nodes[0].children.at(i);
		const auto timer = InfoOf(mind.nodes[top].kind).timer;
		const float value = timer == 0 || turn % timer == 0 ? WorkOut(mind, world, top, turn) : 0.0f;
		if (value > best)
		{
			best = value;
			mind.nodes[0].value = value;
			mind.nodes[0].bestLeaf = mind.nodes[top].bestLeaf;
		}
	}
	return best;
}

bool openblack::computer_mind::PerformDecided(Mind& mind, World& world, uint32_t turn)
{
	const auto leaf = mind.nodes[0].bestLeaf;
	if (!leaf.has_value() || !PerformNode(mind, world, *leaf, turn))
	{
		return false;
	}
	mind.doing = mind.nodes[0].value;
	return true;
}

void openblack::computer_mind::ProcessTurn(Mind& mind, World& world, uint32_t turn)
{
	if (mind.paused || world.HeldByHelp())
	{
		return;
	}
	if (!world.ActionBusy() && !mind.scriptHold)
	{
		if (world.ForceQueued())
		{
			return;
		}
		const bool waited = mind.lastEmptyTurn == 0 || turn - mind.lastEmptyTurn >= k_EmptyWait;
		if (!waited || mind.wasBusy)
		{
			mind.wasBusy = false;
			return;
		}
		mind.workedOut = true;
		if (WorkOut(mind, world, 0, turn) <= 0.0f)
		{
			mind.lastEmptyTurn = turn;
			return;
		}
		world.UnlockAction();
		PerformDecided(mind, world, turn);
		mind.lastEmptyTurn = 0;
		return;
	}

	// The hand does its action. An action that ends badly makes its leaf less wanted.
	if (!world.StepAction() && mind.nodes[0].bestLeaf.has_value())
	{
		mind.nodes[*mind.nodes[0].bestLeaf].suppression *= k_BadEndShare;
	}
	mind.wasBusy = true;
	if (mind.scriptHold)
	{
		return;
	}
	// A need that is wanted more than twice as much as the action stops the action and starts its own
	const float value = WorkOutTopLayer(mind, world, turn);
	if (!world.ActionLocked() && value - mind.doing > mind.doing)
	{
		PerformDecided(mind, world, turn);
	}
}

std::vector<uint16_t> openblack::computer_mind::FindNodes(const Mind& mind, std::string_view name)
{
	std::vector<uint16_t> found;
	ForEachNamed(mind, name, [&found](uint16_t node) { found.push_back(node); });
	return found;
}

bool openblack::computer_mind::SetPersonality(Mind& mind, std::string_view name, float weight)
{
	const auto found = FindNodes(mind, name);
	std::ranges::for_each(found, [&mind, weight](uint16_t node) { mind.nodes[node].weight = weight; });
	return !found.empty();
}

bool openblack::computer_mind::SetSuppression(Mind& mind, std::string_view name, float suppression)
{
	const auto found = FindNodes(mind, name);
	std::ranges::for_each(found, [&mind, suppression](uint16_t node) { mind.nodes[node].suppression = suppression; });
	return !found.empty();
}
