/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureDecisionTree.h"

#include <cmath>

#include <algorithm>
#include <iterator>
#include <map>
#include <ranges>

#include <fmt/format.h>

#include "Common/PortableMath.h"

using namespace openblack;
using namespace openblack::creature_tree;

namespace
{
/// Below this the examples agree well enough to stop
constexpr float k_SettledEntropy = 0.01f;
constexpr double k_BucketReach = 0.25;
/// The game's float of a third, which the entropy multiplies by
constexpr float k_Third = 0.333333343f;
/// The base 10 logarithm of 2 and the game's constant that changes a base 10 logarithm to base 2. The constant is a little
/// less than the inverse of the logarithm, so the game's base 2 logarithm is a little too small.
constexpr double k_Log10Of2 = 0.30102999566398120;
constexpr double k_GameLog2Of10 = 3.3219279368523034;

constexpr std::array<const char*, k_AttributeCount> k_Names {
    "allegiance", "origin",      "animate",         "player",      "harder than me", "creature type",
    "type",       "life",        "tribe",           "town belief", "town needs",     "town size",
    "desire",     "height",      "spell knowledge", "carrying",    "forest size",    "job",
    "sex",        "object type", "abode type",      "being built", "on fire",
};

using enum Attribute;
constexpr std::array k_Common {Allegiance, Origin, Animate, PlayerNumber, HarderThanMe, CreatureType, Type};
constexpr std::array k_TownSlots {Allegiance,          Origin,        Animate,  PlayerNumber, HarderThanMe, CreatureType, Type,
                                  TownReligiousBelief, TownNeedsMost, TownSize, Tribe};
constexpr std::array k_ForestSlots {Allegiance, Origin, Animate, PlayerNumber, HarderThanMe, CreatureType, Type, ForestSize};
constexpr std::array k_AbodeSlots {Allegiance, Origin,    Animate, PlayerNumber, HarderThanMe,   CreatureType,
                                   Type,       AbodeType, Life,    OnFire,       AbodeBeingBuilt};
constexpr std::array k_VillagerSlots {Allegiance, Origin, Animate,     PlayerNumber, HarderThanMe, CreatureType,
                                      Type,       Sex,    VillagerJob, Life,         OnFire};
constexpr std::array k_CreatureSlots {Allegiance, Origin,         Animate, PlayerNumber,   HarderThanMe, CreatureType,
                                      Type,       DominantDesire, Height,  SpellKnowledge, Carrying};

/// The sum less the share's part of the bits, in the game's order of float steps
float LessShareBits(float sum, size_t count, size_t total)
{
	const auto share = static_cast<float>(count) / static_cast<float>(total);
	if (share <= 0.0f)
	{
		return sum;
	}
	const float part = ShareLog2(share) * share;
	return sum - part;
}

uint32_t RangeOf(Attribute attribute)
{
	return k_ScriptValueRanges.at(static_cast<size_t>(attribute));
}

/// The attributes that a branch below a test of the kind of thing can test: the ones still to test, less the kind, and
/// then the allowed attributes of the branch's own kind that not every thing has
std::vector<Attribute> KindBranchAttributes(std::span<const Attribute> remaining, std::span<const Attribute> allowed,
                                            uint32_t kind)
{
	std::vector<Attribute> attributes;
	std::ranges::copy_if(remaining, std::back_inserter(attributes), [](Attribute a) { return a != Attribute::Type; });
	for (const auto attribute : SlotsFor(kind))
	{
		if (!IsCommon(attribute) && std::ranges::find(allowed, attribute) != allowed.end())
		{
			attributes.push_back(attribute);
		}
	}
	return attributes;
}

/// Grows the node for a set of examples, with the attributes still to test, appending it and its children
size_t Grow(Tree& tree, std::vector<Episode> episodes, std::vector<Attribute> remaining, std::span<const Attribute> allowed)
{
	const auto index = tree.nodes.size();
	tree.nodes.emplace_back();
	tree.nodes[index].examples = episodes.size();
	if (episodes.empty())
	{
		return index;
	}
	tree.nodes[index].bucket = BucketOf(MeanFeedback(episodes));
	if (remaining.empty() || Entropy(episodes) < k_SettledEntropy)
	{
		return index;
	}
	std::optional<Attribute> best;
	float bestGain = 0.0f;
	for (const auto attribute : remaining)
	{
		const auto gain = Gain(episodes, attribute);
		if (gain > bestGain)
		{
			bestGain = gain;
			best = attribute;
		}
	}
	if (!best.has_value())
	{
		return index;
	}
	const auto range = RangeOf(*best);
	std::map<uint32_t, std::vector<Episode>> split;
	for (auto& episode : episodes)
	{
		const auto value = *episode.belief.Value(*best);
		if (value < range)
		{
			split[value].push_back(std::move(episode));
		}
	}
	tree.nodes[index].test = best;
	tree.nodes[index].branches = range;
	for (auto& [value, subset] : split)
	{
		auto attributes = remaining;
		if (*best == Attribute::Type)
		{
			attributes = KindBranchAttributes(remaining, allowed, value);
		}
		else
		{
			std::erase(attributes, *best);
		}
		const auto child = Grow(tree, std::move(subset), std::move(attributes), allowed);
		tree.nodes[index].children.emplace_back(value, child);
	}
	return index;
}

void DescribeNode(const Tree& tree, size_t index, size_t depth, std::vector<std::string>& lines, const std::string& prefix)
{
	const auto& node = tree.nodes.at(index);
	const std::string indent(depth * 2, ' ');
	if (!node.test.has_value())
	{
		lines.push_back(fmt::format("{}{}{:+.1f} ({} examples)", indent, prefix, k_Buckets.at(node.bucket), node.examples));
		return;
	}
	lines.push_back(fmt::format("{}{}{}?", indent, prefix, Name(*node.test)));
	for (const auto& [value, child] : node.children)
	{
		DescribeNode(tree, child, depth + 1, lines, fmt::format("{} -> ", value));
	}
}
} // namespace

const char* creature_tree::Name(Attribute attribute)
{
	const auto index = static_cast<size_t>(attribute);
	return index < k_Names.size() ? k_Names.at(index) : "none";
}

const char* creature_tree::BeliefName(uint32_t type)
{
	switch (type)
	{
	case belief_types::k_Town:
		return "town";
	case belief_types::k_Forest:
		return "forest";
	case belief_types::k_Citadel:
		return "citadel";
	case belief_types::k_Abode:
		return "abode";
	case belief_types::k_CitadelPart:
		return "citadel part";
	case belief_types::k_Tree:
		return "tree";
	case belief_types::k_Villager:
		return "villager";
	case belief_types::k_Animal:
		return "animal";
	case belief_types::k_Creature:
		return "creature";
	case belief_types::k_Spell:
		return "spell";
	case belief_types::k_Field:
		return "field";
	case belief_types::k_Rock:
		return "rock";
	case belief_types::k_Poo:
		return "poo";
	case belief_types::k_MagicFood:
		return "food";
	case belief_types::k_Feature:
		return "feature";
	case belief_types::k_Pot:
		return "pot";
	case belief_types::k_SpellIcon:
		return "spell icon";
	case belief_types::k_SpellSeed:
		return "spell seed";
	case belief_types::k_MobileObject:
		return "mobile object";
	case belief_types::k_TotemStatue:
		return "totem";
	case belief_types::k_Flock:
		return "flock";
	default:
		return "object";
	}
}

std::span<const Attribute> creature_tree::SlotsFor(uint32_t beliefType)
{
	switch (beliefType)
	{
	case belief_types::k_Town:
		return k_TownSlots;
	case belief_types::k_Forest:
		return k_ForestSlots;
	case belief_types::k_Abode:
		return k_AbodeSlots;
	case belief_types::k_Villager:
		return k_VillagerSlots;
	case belief_types::k_Creature:
		return k_CreatureSlots;
	default:
		return k_Common;
	}
}

Belief creature_tree::FromSlots(uint32_t type, std::span<const uint32_t> slots)
{
	Belief belief {.type = type};
	const auto names = SlotsFor(type);
	for (size_t i = 0; i < slots.size() && i < names.size(); ++i)
	{
		belief.Set(names[i], slots[i]);
	}
	return belief;
}

bool creature_tree::IsCommon(Attribute attribute)
{
	return attribute <= Attribute::Type;
}

std::vector<uint32_t> creature_tree::ToSlots(const Belief& belief)
{
	std::vector<uint32_t> slots;
	for (const auto attribute : SlotsFor(belief.type))
	{
		slots.push_back(belief.Value(attribute).value_or(0));
	}
	return slots;
}

Belief creature_tree::CommonBelief(const CommonFacts& facts)
{
	Belief belief {.type = facts.type};
	belief.Set(Allegiance, facts.player == facts.creaturesPlayer ? allegiance::k_Friendly : allegiance::k_Enemy);
	belief.Set(Origin, facts.origin);
	belief.Set(Animate, facts.animate ? 1 : 0);
	belief.Set(PlayerNumber, std::min(facts.player.value_or(k_NoPlayerNumber), k_NoPlayerNumber));
	belief.Set(HarderThanMe, facts.harderThanMe ? 1 : 0);
	belief.Set(CreatureType, facts.creatureType);
	belief.Set(Type, facts.type);
	return belief;
}

uint32_t creature_tree::ForestSize(size_t trees)
{
	constexpr size_t k_SmallBelow = 10;
	constexpr size_t k_MiddleBelow = 20;
	if (trees < k_SmallBelow)
	{
		return 0;
	}
	return trees < k_MiddleBelow ? 1 : 2;
}

uint32_t creature_tree::CreatureHeight(float shownSize)
{
	constexpr std::array k_Limits {0.25f, 0.75f, 1.25f};
	const auto taller = std::ranges::count_if(k_Limits, [shownSize](float limit) { return shownSize >= limit; });
	return static_cast<uint32_t>(taller);
}

uint32_t creature_tree::LifeValue(float life)
{
	constexpr float k_LowLife = 0.2f;
	return life > k_LowLife ? 1 : 0;
}

uint32_t creature_tree::OnFireValue(bool onFire)
{
	return onFire ? 0 : 1;
}

uint32_t creature_tree::BeingBuiltValue(bool built)
{
	return built ? 1 : 0;
}

uint32_t creature_tree::DominantDesireValue(uint32_t shownDesire)
{
	constexpr uint32_t k_LastDesireValue = 40;
	return std::min(shownDesire, k_LastDesireValue);
}

uint32_t creature_tree::SpellKnowledgeValue(size_t miraclesKnown)
{
	constexpr size_t k_LastKnowledgeValue = 42;
	return static_cast<uint32_t>(std::min(miraclesKnown, k_LastKnowledgeValue));
}

uint32_t creature_tree::CarryingValue(std::optional<uint32_t> objectType)
{
	constexpr uint32_t k_LastObjectType = 44;
	return std::min(objectType.value_or(k_CarriesNothing), k_LastObjectType);
}

void creature_tree::AddEpisode(std::vector<Episode>& episodes, Episode episode)
{
	if (episodes.size() >= k_MaxEpisodes)
	{
		episodes.resize(k_MaxEpisodes - 1);
	}
	episodes.push_back(std::move(episode));
}

std::optional<size_t> creature_tree::StepOf(float feedback)
{
	for (size_t i = 0; i < k_Buckets.size(); ++i)
	{
		const float apart = std::abs(feedback - k_Buckets.at(i));
		if (static_cast<double>(apart) <= k_BucketReach)
		{
			return i;
		}
	}
	return std::nullopt;
}

size_t creature_tree::BucketOf(float feedback)
{
	return StepOf(feedback).value_or(feedback < 0.0f ? 0 : k_Buckets.size() - 1);
}

float creature_tree::ShareLog2(float share)
{
	constexpr double k_Scale = k_Log10Of2 * k_GameLog2Of10;
	return static_cast<float>(portable_math::Log2(static_cast<double>(share)) * k_Scale);
}

float creature_tree::MeanFeedback(std::span<const Episode> episodes)
{
	if (episodes.empty())
	{
		return 0.0f;
	}
	// The game keeps the newest example first in its list
	float sum = 0.0f;
	for (const auto& episode : std::views::reverse(episodes))
	{
		sum += episode.feedback;
	}
	return sum / static_cast<float>(episodes.size());
}

float creature_tree::Entropy(std::span<const Episode> episodes)
{
	if (episodes.empty())
	{
		return 0.0f;
	}
	std::array<size_t, 2> signs {};
	std::array<size_t, k_Buckets.size()> buckets {};
	for (const auto& episode : episodes)
	{
		++signs.at(episode.feedback < 0.0f ? 0 : 1);
		if (const auto step = StepOf(episode.feedback))
		{
			++buckets.at(*step);
		}
	}
	float sign = 0.0f;
	for (const auto count : signs)
	{
		sign = LessShareBits(sign, count, episodes.size());
	}
	float bucket = 0.0f;
	for (const auto count : buckets)
	{
		bucket = LessShareBits(bucket, count, episodes.size());
	}
	const float sum = (sign + sign) + bucket;
	return sum * k_Third;
}

float creature_tree::Gain(std::span<const Episode> episodes, Attribute attribute)
{
	if (episodes.empty())
	{
		return 0.0f;
	}
	const auto range = RangeOf(attribute);
	std::map<uint32_t, std::vector<Episode>> split;
	for (const auto& episode : episodes)
	{
		const auto value = episode.belief.Value(attribute);
		if (!value.has_value())
		{
			return 0.0f;
		}
		if (*value < range)
		{
			split[*value].push_back(episode);
		}
	}
	float remainder = 0.0f;
	float splitInfo = 0.0f;
	for (const auto& [value, subset] : split)
	{
		const auto share = static_cast<float>(subset.size()) / static_cast<float>(episodes.size());
		remainder += Entropy(subset) * share;
		splitInfo = LessShareBits(splitInfo, subset.size(), episodes.size());
	}
	return (Entropy(episodes) - remainder) / std::max(splitInfo, 1.0f);
}

Tree creature_tree::Build(std::span<const Episode> episodes, std::span<const Attribute> allowed)
{
	Tree tree;
	if (episodes.empty())
	{
		return tree;
	}
	std::vector<Attribute> common;
	std::ranges::copy_if(allowed, std::back_inserter(common), IsCommon);
	Grow(tree, {episodes.begin(), episodes.end()}, std::move(common), allowed);
	return tree;
}

std::optional<Leaf> creature_tree::LeafOf(const Tree& tree, const Belief& belief)
{
	if (tree.nodes.empty())
	{
		return std::nullopt;
	}
	size_t index = 0;
	while (true)
	{
		const auto& node = tree.nodes.at(index);
		if (!node.test.has_value())
		{
			return Leaf {.node = index};
		}
		const auto value = belief.Value(*node.test);
		if (!value.has_value() || *value >= node.branches)
		{
			return std::nullopt;
		}
		const auto found = std::ranges::find(node.children, *value, &std::pair<uint32_t, size_t>::first);
		if (found == node.children.end())
		{
			return Leaf {.node = index, .emptyBranch = true};
		}
		index = found->second;
	}
}

float creature_tree::Evaluate(const Tree& tree, const Belief& belief)
{
	const auto leaf = LeafOf(tree, belief);
	return leaf.has_value() ? k_Buckets.at(tree.nodes.at(leaf->node).bucket) : 0.0f;
}

float creature_tree::Usefulness(float utility)
{
	const float usefulness = utility >= 0.0f ? 0.1f + 0.9f * utility : 0.1f * (utility + 1.0f);
	return std::clamp(usefulness, 0.0f, k_MostUsefulness);
}

Tree creature_tree::DistinctionTree(Attribute attribute, uint32_t value)
{
	const auto range = k_ScriptValueRanges.at(static_cast<size_t>(attribute));
	Tree tree;
	tree.nodes.reserve(static_cast<size_t>(range) + 1);
	tree.nodes.push_back({.test = attribute, .branches = range});
	tree.nodes.front().children.reserve(range);
	for (uint32_t each = 0; each < range; ++each)
	{
		tree.nodes.front().children.emplace_back(each, tree.nodes.size());
		tree.nodes.push_back({.bucket = each == value ? k_Buckets.size() - 1 : 0});
	}
	return tree;
}

std::vector<std::string> creature_tree::Describe(const Tree& tree)
{
	std::vector<std::string> lines;
	if (!tree.nodes.empty())
	{
		DescribeNode(tree, 0, 0, lines, "");
	}
	return lines;
}

bool creature_tree::KeptFromFirstSight(Attribute attribute)
{
	switch (attribute)
	{
	case Attribute::Origin:
	case Attribute::Animate:
	case Attribute::PlayerNumber:
	case Attribute::Type:
	case Attribute::Tribe:
	case Attribute::TownSize:
	case Attribute::VillagerJob:
	case Attribute::Sex:
	case Attribute::MobileObjectType:
	case Attribute::AbodeType:
		return true;
	default:
		return false;
	}
}

creature_tree::Belief creature_tree::WithFirstSight(const Belief& firstSight, const Belief& now)
{
	auto belief = firstSight;
	for (size_t i = 0; i < k_AttributeCount; ++i)
	{
		const auto attribute = static_cast<Attribute>(i);
		if (belief.values.at(i).has_value() && !KeptFromFirstSight(attribute) && now.values.at(i).has_value())
		{
			belief.values.at(i) = now.values.at(i);
		}
	}
	return belief;
}
