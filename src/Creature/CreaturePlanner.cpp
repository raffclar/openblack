/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreaturePlanner.h"

#include <algorithm>
#include <vector>

using namespace openblack;
using namespace openblack::creature_planner;
using creature_desires::Desire;

namespace
{
/// The action's base priority, and how much each step of opinion adds
constexpr float k_BaseActionPriority = 0.5f;
constexpr float k_OpinionWeight = 0.0025f;
constexpr float k_NoveltyGrowth = 4.0f;
constexpr float k_FirstDesireNovelty = 0.025f;
} // namespace

float creature_planner::DistancePriority(bool held, float distance, float distanceWeight)
{
	if (held)
	{
		return k_HeldPriority;
	}
	return 1.0f - distanceWeight * std::min(distance, k_MaxGoalDistance) / k_MaxGoalDistance;
}

float creature_planner::ActionPriority(float opinion, bool disabled)
{
	return disabled ? 0.0f : k_BaseActionPriority + k_OpinionWeight * (opinion + 1.0f);
}

float creature_planner::Novelty(uint32_t turnsSinceDone, bool servesFirstDesire)
{
	const auto share = static_cast<float>(std::min(turnsSinceDone, k_NoveltyTurns)) / static_cast<float>(k_NoveltyTurns);
	return std::clamp(k_NoveltyGrowth * share * (servesFirstDesire ? k_FirstDesireNovelty : 1.0f), 0.0f, k_MaxNovelty);
}

float creature_planner::Priority(float desire, std::optional<float> goalUsefulness, float actionPriority,
                                 float objectUsefulness, std::optional<float> usedUsefulness)
{
	return desire * goalUsefulness.value_or(k_DefaultUsefulness) * std::min(actionPriority, k_MaxActionPriority) *
	       objectUsefulness * usedUsefulness.value_or(k_DefaultUsefulness) * k_PriorityScale;
}

size_t creature_planner::GoalsPerTurn(size_t creatureCount)
{
	return creatureCount < k_CreaturesForFewerGoals ? k_GoalsPerTurn : 1;
}

bool creature_planner::ShouldSwitch(float best, float current)
{
	return best - current > current;
}

bool creature_planner::CountsActedOn(Desire desire, bool aboutSomething)
{
	return !aboutSomething || desire == Desire::Anger;
}

namespace
{
/// How much the creature wants to take an action: its priority and its novelty
float Wanted(const ActionCandidate& action)
{
	return ActionPriority(action.opinion) + Novelty(action.turnsSinceDone, action.servesFirstDesire);
}

/// An action that the plan tried and could not use is not tried again, in any place of the list
void MarkTried(std::span<const ActionCandidate> actions, uint32_t action, std::vector<bool>& tried)
{
	for (size_t i = 0; i < actions.size(); ++i)
	{
		if (actions[i].action == action)
		{
			tried.at(i) = true;
		}
	}
}

/// The plan for an action with the thing that it acts on and the thing that it uses
Plan MakePlan(const DesirePlanning& planning, const ActionCandidate& action, const std::optional<ObjectCandidate>& object,
              float objectUsefulness, const std::optional<UsedChoice>& used)
{
	const auto wanted = Wanted(action);
	Plan plan {.desire = planning.desire, .action = action.action, .actionPriority = wanted};
	if (object.has_value() && action.needsObject)
	{
		plan.object = object->id;
		plan.goalUsefulness = object->usefulness;
	}
	if (used.has_value())
	{
		plan.instrument = used->id;
	}
	plan.priority = Priority(planning.strength, planning.activityUsefulness, wanted, objectUsefulness,
	                         used.has_value() ? std::optional(used->usefulness) : std::nullopt);
	return plan;
}

/// The plan when the action comes first, and the thing that it acts on after it
std::optional<Plan> PlanActionFirst(const DesirePlanning& planning)
{
	std::vector<bool> tried(planning.actions.size(), false);
	for (size_t attempt = 0; attempt < k_MostActionsTried; ++attempt)
	{
		std::optional<size_t> chosen;
		float most = 0.0f;
		for (size_t i = 0; i < planning.actions.size(); ++i)
		{
			const auto& action = planning.actions[i];
			if (action.disabled || tried.at(i))
			{
				continue;
			}
			const auto wanted = Wanted(action);
			if (wanted >= most)
			{
				chosen = i;
				most = wanted;
			}
		}
		if (!chosen.has_value())
		{
			return std::nullopt;
		}
		const auto& action = planning.actions[*chosen];
		const auto object = action.needsObject ? planning.findObject(action) : std::nullopt;
		if (action.needsObject && !object.has_value())
		{
			MarkTried(planning.actions, action.action, tried);
			continue;
		}
		const auto used = action.usesThing
		                      ? planning.findUsed(action, object.has_value() ? std::optional(object->id) : std::nullopt)
		                      : std::nullopt;
		if (action.usesThing && !used.has_value())
		{
			MarkTried(planning.actions, action.action, tried);
			continue;
		}
		const auto objectUsefulness =
		    object.has_value() ? object->actedOnUsefulness.value_or(k_DefaultUsefulness) : k_DefaultUsefulness;
		return MakePlan(planning, action, object, objectUsefulness, used);
	}
	return std::nullopt;
}

/// The plan when the action comes first, weighed with the thing that it uses, and the part that it acts on after it
std::optional<Plan> PlanActionThenPart(const DesirePlanning& planning)
{
	std::vector<bool> tried(planning.actions.size(), false);
	for (size_t attempt = 0; attempt < k_MostActionsTried; ++attempt)
	{
		std::optional<size_t> chosen;
		std::optional<UsedChoice> chosenUsed;
		float most = 0.0f;
		for (size_t i = 0; i < planning.actions.size(); ++i)
		{
			const auto& action = planning.actions[i];
			if (action.disabled || tried.at(i))
			{
				continue;
			}
			// An action that has no thing to use is not chosen
			const auto used = action.usesThing ? planning.findUsed(action, std::nullopt) : std::nullopt;
			if (action.usesThing && !used.has_value())
			{
				continue;
			}
			const auto score =
			    Wanted(action) * k_DefaultUsefulness * (used.has_value() ? used->usefulness : k_DefaultUsefulness);
			if (score >= most)
			{
				chosen = i;
				chosenUsed = used;
				most = score;
			}
		}
		if (!chosen.has_value())
		{
			return std::nullopt;
		}
		const auto& action = planning.actions[*chosen];
		const auto part = planning.findObject(action);
		if (!part.has_value())
		{
			MarkTried(planning.actions, action.action, tried);
			continue;
		}
		return MakePlan(planning, action, part, k_DefaultUsefulness, chosenUsed);
	}
	return std::nullopt;
}

/// The plan for the part chosen before it: the action that can act on the part and is wanted the most
std::optional<Plan> PlanPartThenAction(const DesirePlanning& planning)
{
	std::optional<size_t> chosen;
	std::optional<ObjectCandidate> chosenPart;
	std::optional<UsedChoice> chosenUsed;
	float most = 0.0f;
	for (size_t i = 0; i < planning.actions.size(); ++i)
	{
		const auto& action = planning.actions[i];
		if (action.disabled)
		{
			continue;
		}
		const auto part = planning.findObject(action);
		if (!part.has_value())
		{
			continue;
		}
		const auto used = action.usesThing ? planning.findUsed(action, part->id) : std::nullopt;
		if (action.usesThing && !used.has_value())
		{
			continue;
		}
		const auto score = Wanted(action) * part->actedOnUsefulness.value_or(k_DefaultUsefulness) *
		                   (used.has_value() ? used->usefulness : k_DefaultUsefulness);
		if (score >= most)
		{
			chosen = i;
			chosenPart = part;
			chosenUsed = used;
			most = score;
		}
	}
	if (!chosen.has_value())
	{
		return std::nullopt;
	}
	return MakePlan(planning, planning.actions[*chosen], chosenPart,
	                chosenPart->actedOnUsefulness.value_or(k_DefaultUsefulness), chosenUsed);
}
} // namespace

std::optional<Plan> creature_planner::PlanDesire(const DesirePlanning& planning)
{
	switch (planning.order)
	{
	case PlanOrder::ActionFirst:
		return PlanActionFirst(planning);
	case PlanOrder::ActionThenPart:
		return PlanActionThenPart(planning);
	case PlanOrder::PartThenAction:
		return PlanPartThenAction(planning);
	}
	return std::nullopt;
}

std::optional<Desire> creature_planner::BestDesireOnThing(std::span<const DesireOnThing> desires)
{
	std::optional<Desire> best;
	float bestUsefulness = 0.0f;
	for (const auto& weighed : desires)
	{
		if (weighed.strength * weighed.usefulness > bestUsefulness)
		{
			best = weighed.desire;
			bestUsefulness = weighed.usefulness;
		}
	}
	return best;
}

std::optional<UsedChoice> creature_planner::ChooseUsed(std::span<const UsedCandidate> objects, float distanceWeight,
                                                       std::optional<uint32_t> goal)
{
	std::optional<UsedChoice> chosen;
	float best = 0.0f;
	for (const auto& object : objects)
	{
		if (object.id == goal)
		{
			continue;
		}
		const auto score =
		    std::clamp(DistancePriority(object.held, object.distance, distanceWeight) * k_DefaultUsefulness, 0.0f, 1.0f);
		if (score > best)
		{
			best = score;
			chosen = UsedChoice {.id = object.id, .usefulness = score};
		}
	}
	return chosen;
}

std::vector<Desire> creature_planner::NextGoals(PlannerState& state,
                                                const std::array<bool, creature_desires::k_DesireCount>& eligible, size_t count)
{
	std::vector<Desire> goals;
	for (size_t tried = 0; tried < eligible.size() && goals.size() < count; ++tried)
	{
		const auto index = state.nextGoal % eligible.size();
		state.nextGoal = index + 1;
		if (eligible.at(index))
		{
			goals.push_back(static_cast<Desire>(index));
		}
		else
		{
			// A desire that can't be planned loses its old plan
			state.best.at(index).reset();
		}
	}
	return goals;
}

std::optional<Plan> creature_planner::Best(const PlannerState& state)
{
	std::optional<Plan> best;
	for (const auto& plan : state.best)
	{
		if (plan.has_value() && (!best.has_value() || plan->priority > best->priority))
		{
			best = plan;
		}
	}
	return best;
}

std::optional<Plan> creature_planner::Choose(const PlannerState& state)
{
	const auto best = Best(state);
	if (!best.has_value())
	{
		return std::nullopt;
	}
	const auto current = state.current.has_value() ? state.current->priority : 0.0f;
	if (state.current.has_value() && state.current->desire == best->desire && state.current->action == best->action &&
	    state.current->object == best->object)
	{
		return std::nullopt;
	}
	return ShouldSwitch(best->priority, current) ? best : std::nullopt;
}
