/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerMind.h"

#include <cctype>
#include <cstdint>

#include <algorithm>
#include <bit>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Common/GUtilsDistance.h"

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

/// Starts the action of the node itself. The suppression of the node goes down after the start, and much more when the
/// action cannot start. False when the node has no action or the action cannot start.
bool PerformOwn(Mind& mind, World& world, uint16_t node)
{
	const auto& info = InfoOf(mind.nodes[node].kind);
	if (!info.perform)
	{
		return false;
	}
	world.ClearAction();
	if (world.Perform(mind, node))
	{
		mind.nodes[node].suppression = info.afterPerform;
		world.StartAction();
		return true;
	}
	mind.nodes[node].suppression = k_FailedSuppression;
	return false;
}

/// Starts the action of the node, or of the first node above that has an action
bool PerformNode(Mind& mind, World& world, uint16_t node, uint32_t turn)
{
	for (std::optional<uint16_t> at = node; at.has_value(); at = mind.nodes[*at].parent)
	{
		mind.nodes[*at].lastPerformed = turn;
		if (InfoOf(mind.nodes[*at].kind).perform)
		{
			return PerformOwn(mind, world, *at);
		}
	}
	return false;
}

/// Starts the action of the leaf that the node chose. False when the node chose no leaf.
bool PerformBestLeaf(Mind& mind, World& world, uint16_t node, uint32_t turn)
{
	const auto leaf = mind.nodes[node].bestLeaf;
	return leaf.has_value() && PerformNode(mind, world, *leaf, turn);
}

/// The name starts with the letters of a direct action, in capitals or not
[[nodiscard]] bool DirectName(std::string_view name)
{
	constexpr std::string_view k_Direct = "DIRECT";
	if (name.size() < k_Direct.size())
	{
		return false;
	}
	return std::ranges::equal(name.substr(0, k_Direct.size()), k_Direct, [](char a, char b) {
		return std::toupper(static_cast<unsigned char>(a)) == static_cast<unsigned char>(b);
	});
}

/// A script action at a script marker: the node keeps the place of the marker. A node with an action gives the place to
/// its first child and chooses that child. A node with no action looks at its children.
bool ForceAtPlace(Mind& mind, World& world, uint16_t node, entt::entity first, glm::vec3 place, float value, uint32_t turn)
{
	auto& self = mind.nodes[node];
	self.place = place;
	self.object = first;
	self.value = value;
	if (InfoOf(self.kind).perform)
	{
		if (self.childCount != 0)
		{
			const auto child = self.children.at(0);
			mind.nodes[child].place = place;
			mind.nodes[node].bestLeaf = child;
		}
	}
	else
	{
		WorkOutChildren(mind, world, node, turn);
	}
	return PerformBestLeaf(mind, world, node, turn);
}

/// A script action with two things: the node acts on the first thing. A node with an action gives the second thing to
/// its first child, or, with no children, acts on the second thing and gives the first thing to the node above.
bool ForceWithThings(Mind& mind, World& world, uint16_t node, entt::entity first, entt::entity second, float value,
                     uint32_t turn)
{
	mind.nodes[node].value = value;
	mind.nodes[node].object = first;
	if (!InfoOf(mind.nodes[node].kind).perform)
	{
		WorkOutChildren(mind, world, node, turn);
		if (mind.nodes[node].bestLeaf == node)
		{
			return false;
		}
		return PerformBestLeaf(mind, world, node, turn);
	}
	if (mind.nodes[node].childCount != 0)
	{
		if (second == entt::null)
		{
			WorkOutChildren(mind, world, node, turn);
		}
		else
		{
			const auto child = mind.nodes[node].children.at(0);
			mind.nodes[child].object = second;
			mind.nodes[child].bestLeaf = child;
			mind.nodes[node].bestLeaf = child;
		}
	}
	else if (const auto parent = mind.nodes[node].parent; parent.has_value())
	{
		mind.nodes[*parent].object = first;
		mind.nodes[node].object = second;
	}
	return PerformBestLeaf(mind, world, node, turn);
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
		self.siteTown = entt::null;
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
		if (ForceNext(mind, world, turn))
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
	// The action of a script ends with the last step of the hand
	if (!world.ActionBusy())
	{
		mind.scriptHand = false;
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

bool openblack::computer_mind::ForceAction(Mind& mind, World& world, std::string_view name, entt::entity first,
                                           entt::entity second, float value, uint32_t turn)
{
	const auto found = FindNodes(mind, name);
	if (found.empty())
	{
		return false;
	}
	const auto node = found.front();
	// A direct action uses the two things itself and starts at once
	if (DirectName(name))
	{
		mind.nodes[node].object = first;
		mind.nodes[node].argument = second;
		if (PerformOwn(mind, world, node))
		{
			mind.scriptHand = true;
			return true;
		}
	}
	const auto place = world.MarkerPlace(second);
	const bool started = place.has_value() ? ForceAtPlace(mind, world, node, first, *place, value, turn)
	                                       : ForceWithThings(mind, world, node, first, second, value, turn);
	if (started)
	{
		mind.scriptHand = true;
	}
	return started;
}

void openblack::computer_mind::QueueAction(Mind& mind, std::string_view name, entt::entity first, entt::entity second,
                                           uint32_t turn)
{
	mind.queue.push_back(
	    {.name = std::string(name.substr(0, k_QueuedNameLength)), .first = first, .second = second, .turn = turn});
}

bool openblack::computer_mind::ForceNext(Mind& mind, World& world, uint32_t turn)
{
	if (mind.queue.empty())
	{
		return false;
	}
	const auto next = mind.queue.front();
	const bool started = ForceAction(mind, world, next.name, next.first, next.second, 1.0f, turn);
	mind.queue.erase(mind.queue.begin());
	return started;
}

void openblack::computer_mind::SeeNearHand(std::vector<SeenThing>& seen, const map_coords::MapCoords& hand, uint32_t turn,
                                           const SeenWorld& world)
{
	if (seen.size() > k_SeenMost && turn % k_SeenForgetEvery == 0)
	{
		std::erase_if(seen, [turn](const SeenThing& old) { return turn - old.turn > k_SeenForgetAfter; });
	}
	// The god looks at the cells in a spiral out from its hand. It stops at the first cell on the map that is too far.
	auto coords = hand;
	map_coords::Spiral spiral;
	for (int32_t cells = k_SeenMostCells; cells != 0; --cells)
	{
		if (const glm::ivec2 cell = map_coords::Cell(coords); map_coords::InBounds(cell))
		{
			if (!(gutils::GetDistanceInMetres(hand, coords) < k_SeenCellMargin + k_SeenRadius))
			{
				return;
			}
			for (const auto object : world.ThingsIn(cell))
			{
				if (!world.Available(object))
				{
					continue;
				}
				const auto thing = world.SeenAs(object);
				if (!world.Available(thing) ||
				    std::ranges::any_of(seen, [thing](const SeenThing& old) { return old.thing == thing; }))
				{
					continue;
				}
				seen.insert(seen.begin(), {.thing = thing, .turn = turn});
			}
		}
		map_coords::AddCells(coords, spiral.Next());
	}
}

std::string openblack::computer_mind::PersonalityName(NodeKind kind)
{
	const auto& info = InfoOf(kind);
	std::string name(info.name);
	if (info.text.has_value())
	{
		name += ": " + std::to_string(*info.text) + " ";
	}
	return name;
}

std::vector<uint8_t> openblack::computer_mind::PersonalityFile(const Mind& mind)
{
	// The nodes of a mind are in the order of the file: a node, then the whole branch of each child in turn
	std::vector<uint8_t> bytes;
	for (const auto& node : mind.nodes)
	{
		const auto name = PersonalityName(node.kind);
		bytes.insert(bytes.end(), name.begin(), name.end());
		bytes.push_back(0);
		const auto bits = std::bit_cast<uint32_t>(node.weight);
		for (uint32_t shift = 0; shift < 32; shift += 8)
		{
			bytes.push_back(static_cast<uint8_t>(bits >> shift));
		}
	}
	return bytes;
}

void openblack::computer_mind::ReadPersonalityFile(Mind& mind, std::span<const uint8_t> bytes)
{
	size_t at = 0;
	for (auto& node : mind.nodes)
	{
		// The load skips the name with its zero byte. It does not compare the name with the name in the file.
		at = std::min(bytes.size(), at + PersonalityName(node.kind).size() + 1);
		auto bits = std::bit_cast<uint32_t>(node.weight);
		for (uint32_t shift = 0; shift < 32 && at < bytes.size(); shift += 8, ++at)
		{
			bits = (bits & ~(0xFFu << shift)) | (static_cast<uint32_t>(bytes[at]) << shift);
		}
		node.weight = std::bit_cast<float>(bits);
		if (at >= bytes.size())
		{
			return;
		}
	}
}
