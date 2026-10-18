/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureTurnOrder.h"

#include <cstddef>

using namespace openblack;

void creature_turn_order::HoldBodyDesires(creature_desires::Desires& desires,
                                          const std::array<bool, creature_desires::k_DesireCount>& body, uint32_t phase)
{
	if (phase >= k_FirstStageWithBody)
	{
		return;
	}
	for (size_t d = 0; d < body.size(); ++d)
	{
		if (body.at(d))
		{
			desires.desires.at(d).value = 0.0f;
		}
	}
}

creature_turn_order::HeldTurn creature_turn_order::WhileHeld(bool held, bool showDesireOnHold)
{
	if (!held)
	{
		return HeldTurn::Think;
	}
	return showDesireOnHold ? HeldTurn::ShowDesire : HeldTurn::StandStill;
}

bool creature_turn_order::RestartsAfterHold(bool heldSinceRestart, bool planHasSteps)
{
	return heldSinceRestart && planHasSteps;
}

bool creature_turn_order::OwnPlanRuns(bool planHasSteps, creature_desires::Desire desire)
{
	return planHasSteps && desire != creature_desires::Desire::FollowPlayerDesire;
}

bool creature_turn_order::StepCopyTimeout(uint32_t& turnsWithoutOwnPlan, bool ownPlanSinceStop, uint32_t wholeTurnsPerSecond)
{
	if (!ownPlanSinceStop)
	{
		++turnsWithoutOwnPlan;
	}
	// The limit is a whole number of turns, with the fraction removed
	const auto limit = static_cast<uint32_t>(static_cast<float>(wholeTurnsPerSecond) * k_CopyTimeoutSeconds);
	if (turnsWithoutOwnPlan <= limit)
	{
		return false;
	}
	turnsWithoutOwnPlan = 0;
	return true;
}

creature_turn_order::Completion creature_turn_order::CompletionOf(creature_desires::Desire desire, bool forced,
                                                                  bool forcedCounts, bool fullRecovery, float life)
{
	Completion completion {.counts = !forced || forcedCounts};
	if (!completion.counts)
	{
		return completion;
	}
	if (desire == creature_desires::Desire::RestoreHealth)
	{
		// The life can go above the full life
		completion.life = life + k_RestoreHealthLife;
	}
	if (fullRecovery)
	{
		completion.life = k_FullLife;
		completion.climateTemperature = true;
	}
	return completion;
}

bool creature_turn_order::StopsReactionBeforeDecision(const DecisionState& state)
{
	return !state.planRuns && state.reacting;
}

bool creature_turn_order::SkipsDecision(const DecisionState& state)
{
	// With no agenda that runs, the forced mark is clear and the reaction stopped
	if (!state.planRuns)
	{
		return false;
	}
	return state.planForced || state.reacting;
}
