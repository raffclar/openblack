/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureMindTables.h"

#include <cstring>

#include <algorithm>
#include <iterator>
#include <vector>

#include "InfoConstants.h"

using namespace openblack;
using namespace openblack::creature_mind_tables;

namespace
{
/// The tables mark unused slots of the action lists with 0 (no action) or with a number past the last action
constexpr uint32_t k_NoAction = 0;
/// The planner reads at most this many entries of the list of actions of a desire
constexpr size_t k_MostActionsInList = 30;
/// The action table's marks for an action that needs no skill (one past the last) or no miracle
constexpr uint32_t k_NoSkill = 6;
constexpr uint32_t k_NoMiracle = 0;

std::optional<size_t> SkillNeeded(uint32_t skill)
{
	return skill != k_NoSkill ? std::optional<size_t>(skill) : std::nullopt;
}

constexpr std::array<float, 17> k_MiracleMultipliers {1.0f, 1.7f,  1.5f, 1.2f, 1.1f, 1.3f, 1.4f, 1.2f, 1.4f,
                                                      1.3f, 1.35f, 1.9f, 0.9f, 4.0f, 1.2f, 1.5f, 1.1f};

template <size_t N>
std::string Text(const std::array<char, N>& chars)
{
	return {chars.data(), strnlen(chars.data(), chars.size())};
}

/// A table row of 32-bit values read as its numbers
template <typename T, typename Row>
std::vector<T> Words(const Row& row)
{
	static_assert(sizeof(Row) % sizeof(T) == 0);
	std::vector<T> values(sizeof(Row) / sizeof(T));
	std::memcpy(values.data(), &row, sizeof(Row));
	return values;
}
} // namespace

Tables creature_mind_tables::Build(const InfoConstants& info)
{
	Tables tables;
	for (const auto& row : info.creatureAction)
	{
		tables.actions.push_back({
		    .name = Text(row.name),
		    .desire = row.desire,
		    .desireMultiplier = row.desireMultiplier,
		    .alwaysApplies = row.alwaysApplies != 0,
		    .learningWindowSeconds = row.learningWindowSeconds,
		    .learnable = row.learnable != 0,
		    .eatWhenStroked = row.eatWhenStroked != 0,
		    .actsOnThing = row.actsOnThing != 0,
		    .notOnWhatItHolds = row.notOnWhatItHolds != 0,
		    .shownDesire = row.shownDesire,
		    .creatureNicenessChange = row.creatureNicenessChange,
		    .startingOpinion = row.startingOpinion,
		    .fullRecovery = row.fullRecovery != 0,
		    .needs = {.skills = {SkillNeeded(row.skillsNeeded[0]), SkillNeeded(row.skillsNeeded[1])},
		              .miracle = row.magicType != k_NoMiracle ? std::optional<size_t>(row.magicType) : std::nullopt},
		});
	}
	const auto actionCount = static_cast<uint32_t>(tables.actions.size());
	for (size_t d = 0; d < k_DesireCount; ++d)
	{
		// The list ends at the first empty entry, and keeps an action that is in it more than once, as the planner reads
		// the entries in their order
		for (const auto action : Words<uint32_t>(info.creatureDesireAction1.at(d)))
		{
			if (action == k_NoAction || tables.desireActions.at(d).size() == k_MostActionsInList)
			{
				break;
			}
			if (action < actionCount)
			{
				tables.desireActions.at(d).push_back(action);
			}
		}
		const auto& initial = info.creatureInitialDesire.at(d);
		tables.rules.at(d) = {
		    .initialMax = initial.initialMax,
		    .decayMin = initial.decayMin,
		    .decayMax = initial.decayMax,
		    .initialWeight = initial.initialWeight,
		    .learnsWeight = initial.learnsWeight != 0,
		    .lessonDivisor = initial.lessonDivisor,
		    .learnable = initial.learnable != 0,
		    .distanceWeight = initial.distanceWeight,
		    .aboutSomething = initial.aboutSomething != 0,
		    .approvesOrders = initial.approvesOrders != 0,
		};
		tables.texts.at(d) = {
		    .desire = Text(initial.desireText),
		    .trying = Text(initial.tryingText),
		    .doing = Text(initial.doingText),
		};
		const auto dependencies = Words<float>(info.creatureDesireDependency.at(d));
		for (size_t e = 0; e < k_DesireCount && e < dependencies.size(); ++e)
		{
			tables.dependencies.at(d).at(e) = dependencies[e];
		}
		for (const auto attribute : Words<uint32_t>(info.creatureDesireAttributeEntry.at(d)))
		{
			if (attribute < creature_tree::k_NoAttribute)
			{
				tables.attributes.at(d).push_back(static_cast<creature_tree::Attribute>(attribute));
			}
		}
	}
	// Each list of the town table runs up to its first empty slot
	for (size_t d = 0; d < tables.townActions.size(); ++d)
	{
		for (const auto action : Words<uint32_t>(info.creatureDesireAction2.at(d)))
		{
			if (action == k_NoAction || action >= actionCount)
			{
				break;
			}
			tables.townActions.at(d).push_back(action);
		}
	}
	for (size_t s = 0; s < tables.thresholdBounds.size() && s < info.creatureSourceBounds.size(); ++s)
	{
		const auto& row = info.creatureSourceBounds.at(s);
		tables.thresholdBounds.at(s) = {.min = row.minThreshold, .max = row.maxThreshold, .step = row.thresholdStep};
	}
	for (const auto& entry : info.creatireActionKnownAboutEntry)
	{
		tables.skills.push_back({
		    .name = Text(entry.name),
		    .watchSeconds = entry.watchSeconds,
		    .minPhase = entry.minPhase,
		});
	}
	for (const auto& entry : info.creatureMagicActionKnownAboutEntry)
	{
		tables.miracles.push_back({
		    .name = Text(entry.name),
		    .timesToSee = entry.timesToSee,
		    .minPhase = entry.minPhase,
		    .teachable = entry.teachable != 0,
		    .action = entry.action,
		    .desire = entry.desire,
		});
	}
	for (const auto& entry : info.creatureMimic)
	{
		creature_watching::MimicRule rule {
		    .name = Text(entry.name),
		    .chance = entry.chance,
		    .needsLearningLeash = entry.needsLearningLeash != 0,
		    .noticeAction = entry.noticeAction,
		    .desire = entry.desire,
		    .stageSteps = entry.stageSteps,
		    .copiesDesire = entry.copiesDesire != 0,
		};
		for (const auto action : entry.copyActions)
		{
			if (action != k_NoAction && action < actionCount)
			{
				rule.copyActions.push_back(action);
			}
		}
		tables.mimics.push_back(std::move(rule));
	}
	return tables;
}

std::optional<uint32_t> creature_mind_tables::FindAction(const Tables& tables, std::string_view name)
{
	const auto found = std::ranges::find(tables.actions, name, &ActionInfo::name);
	if (found == tables.actions.end())
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(std::distance(tables.actions.begin(), found));
}

std::vector<float> creature_mind_tables::StartingOpinions(const Tables& tables)
{
	std::vector<float> opinions;
	opinions.reserve(tables.actions.size());
	std::ranges::transform(tables.actions, std::back_inserter(opinions), &ActionInfo::startingOpinion);
	return opinions;
}

float creature_mind_tables::MiracleMultiplier(size_t speciesRow)
{
	return k_MiracleMultipliers.at(std::min(speciesRow, k_MiracleMultipliers.size() - 1));
}
