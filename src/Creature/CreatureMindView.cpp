/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureMindView.h"

#include <algorithm>
#include <iterator>
#include <numeric>
#include <utility>

using namespace openblack;
using namespace openblack::creature_mind_view;
using creature_desires::Desire;
using creature_desires::k_DesireCount;

namespace
{
/// The names of the kinds of source, in the order of the game's source table
constexpr std::array<std::string_view, creature_desires::k_SourceCount> k_SourceNames {
    "impress: watched the player",
    "impress: saw things",
    "compassion: watched the player",
    "compassion: saw things",
    "compassion: content",
    "compassion: innate kindness",
    "anger: watched the player",
    "anger: saw things",
    "anger: dissatisfied",
    "anger: damage",
    "anger: sadness",
    "anger: innate aggression",
    "play: watched the player",
    "play: watched villagers",
    "hunger: low energy",
    "hunger: watched villagers",
    "hunger: sadness",
    "fear: dark",
    "fear: damage",
    "fear: scary magic",
    "curiosity",
    "poo: amount of poo",
    "tiredness: exhaustion",
    "tiredness: laziness",
    "tiredness: night",
    "tiredness: sadness",
    "tiredness: innate lethargy",
    "idle with the player",
    "wanderlust",
    "puke",
    "build a home",
    "bring things home",
    "water: dehydration",
    "restore health: life",
    "be friends",
    "be friends: innate friendliness",
    "attention: loneliness",
    "attention: no interaction",
    "show its state",
    "show its state: innate communicativeness",
    "get warmer",
    "get colder",
    "scratch",
    "run away from the player",
    "rest",
    "obey the player",
    "illness",
    "obey a creature",
    "sadness",
    "go home",
    "tell the player",
    "play with the player",
    "tell a creature",
    "educate a friend",
    "follow the player's desire",
    "get high",
    "hang around at home",
    "mental illness",
    "miss a friend",
    "look around",
    "steal",
};

/// Whether two plans are for the same desire, action and thing. The priority can change after the start of a plan.
bool SameIdentity(const creature_planner::Plan& a, const creature_planner::Plan& b)
{
	return a.desire == b.desire && a.action == b.action && a.object == b.object;
}
} // namespace

std::string_view creature_mind_view::SourceName(uint32_t type)
{
	return type < k_SourceNames.size() ? k_SourceNames.at(type) : "none";
}

bool creature_mind_view::IsActive(const creature_desires::Desires& desires, size_t desire)
{
	if (desire >= k_DesireCount || !desires.IsActivated(desire))
	{
		return false;
	}
	const auto& state = desires.desires.at(desire);
	const auto drive = std::accumulate(state.sources.begin(), state.sources.end(), 0.0f,
	                                   [](float sum, const creature_desires::Source& source) {
		                                   return sum + creature_desires::Sigmoid(source.threshold, source.value);
	                                   });
	return state.value >= k_ActiveFloor || drive >= k_ActiveFloor;
}

std::vector<SourcePart> creature_mind_view::SourcesOf(const creature_desires::DesireState& desire)
{
	std::vector<SourcePart> parts;
	parts.reserve(desire.sources.size());
	std::ranges::transform(desire.sources, std::back_inserter(parts), [](const creature_desires::Source& source) {
		return SourcePart {
		    .type = source.type,
		    .value = source.value,
		    .threshold = source.threshold,
		    .multiplier = source.multiplier,
		    .part = creature_desires::Sigmoid(source.threshold, source.value),
		    .drive = source.drive,
		};
	});
	return parts;
}

std::vector<DesireRow> creature_mind_view::ActiveDesires(const creature_desires::Desires& desires,
                                                         const std::array<float, k_DesireCount>* before, float turnsPerSecond)
{
	std::vector<DesireRow> rows;
	for (size_t d = 0; d < k_DesireCount; ++d)
	{
		if (!IsActive(desires, d))
		{
			continue;
		}
		const auto& state = desires.desires.at(d);
		DesireRow row {
		    .desire = static_cast<Desire>(d),
		    .value = state.value,
		    .max = state.max,
		    .change = before != nullptr ? std::optional(state.value - before->at(d)) : std::nullopt,
		    .suppressedTurns = state.suppressedTurns,
		    .sources = SourcesOf(state),
		};
		row.drive = std::accumulate(row.sources.begin(), row.sources.end(), 0.0f,
		                            [](float sum, const SourcePart& part) { return sum + part.part; });
		if (state.suppressedTurns == 0 && state.increaseSeconds > 0.0f && turnsPerSecond > 0.0f)
		{
			row.growthPerTurn = row.drive / (turnsPerSecond * state.increaseSeconds);
		}
		rows.push_back(std::move(row));
	}
	std::ranges::stable_sort(rows, [](const DesireRow& a, const DesireRow& b) { return a.value > b.value; });
	// The creature shows the first activated desire with the highest value, as the mind finds it
	const auto dominant = creature_desires::Dominant(desires);
	for (auto& row : rows)
	{
		row.winner = row.desire == dominant && row.value > 0.0f;
	}
	return rows;
}

std::string_view creature_mind_view::Name(Outcome outcome)
{
	switch (outcome)
	{
	case Outcome::Chosen:
		return "chosen";
	case Outcome::Forced:
		return "forced";
	case Outcome::Kept:
		return "kept";
	case Outcome::NotBest:
		return "not the best";
	case Outcome::NotPressing:
		return "not pressing enough";
	case Outcome::NoPlan:
		return "no plan";
	case Outcome::Ended:
		return "ended";
	}
	return "unknown";
}

bool creature_mind_view::SamePlan(const std::optional<creature_planner::Plan>& a,
                                  const std::optional<creature_planner::Plan>& b)
{
	if (a.has_value() != b.has_value())
	{
		return false;
	}
	return !a.has_value() || (SameIdentity(*a, *b) && a->priority == b->priority);
}

const std::array<float, k_DesireCount>* Trace::Before() const
{
	return _samples.size() >= 2 ? &_samples.at(_samples.size() - 2).values : nullptr;
}

void Trace::Clear()
{
	*this = Trace {};
}

void Trace::Add(PlanStep step)
{
	_steps.push_back(std::move(step));
	while (_steps.size() > k_PlanSteps)
	{
		_steps.pop_front();
	}
}

void Trace::Record(uint32_t turn, const creature_desires::Desires& desires, const creature_planner::PlannerState& planner,
                   bool planForced)
{
	Sample sample {.turn = turn};
	std::ranges::transform(desires.desires, sample.values.begin(),
	                       [](const creature_desires::DesireState& state) { return state.value; });
	_samples.push_back(sample);
	while (_samples.size() > k_SampleTurns)
	{
		_samples.pop_front();
	}

	if (_started)
	{
		const auto& current = planner.current;
		const bool currentChanged = !SamePlan(current, _lastCurrent);
		const auto beat = _lastCurrent.has_value() ? std::optional(_lastCurrent->priority) : std::nullopt;
		const auto best = creature_planner::Best(planner);
		bool currentFromPlanner = false;
		for (size_t d = 0; d < k_DesireCount; ++d)
		{
			const auto& plan = planner.best.at(d);
			if (SamePlan(plan, _lastBest.at(d)))
			{
				continue;
			}
			PlanStep step {.turn = turn, .desire = static_cast<Desire>(d), .plan = plan, .current = beat};
			if (!plan.has_value())
			{
				step.outcome = Outcome::NoPlan;
			}
			else if (currentChanged && current.has_value() && SameIdentity(*current, *plan))
			{
				step.outcome = planForced ? Outcome::Forced : Outcome::Chosen;
				currentFromPlanner = true;
			}
			else if (current.has_value() && SameIdentity(*current, *plan))
			{
				step.outcome = Outcome::Kept;
			}
			else if (best.has_value() && best->desire != plan->desire)
			{
				step.outcome = Outcome::NotBest;
				step.best = best->desire;
				step.bestPriority = best->priority;
			}
			else
			{
				step.outcome = Outcome::NotPressing;
			}
			Add(std::move(step));
		}
		// A plan that did not come from the planner's plans of this turn: an order, a reaction or a script
		if (currentChanged && !currentFromPlanner)
		{
			if (current.has_value())
			{
				Add({.turn = turn,
				     .desire = current->desire,
				     .plan = current,
				     .outcome = planForced ? Outcome::Forced : Outcome::Chosen,
				     .current = beat});
			}
			else if (_lastCurrent.has_value())
			{
				Add({.turn = turn, .desire = _lastCurrent->desire, .plan = _lastCurrent, .outcome = Outcome::Ended});
			}
		}
	}
	_started = true;
	_lastBest = planner.best;
	_lastCurrent = planner.current;
}
