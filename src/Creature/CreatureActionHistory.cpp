/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureActionHistory.h"

#include <algorithm>

using namespace openblack;
using namespace openblack::creature_action_history;

std::vector<uint32_t> creature_action_history::Fresh(size_t actionCount, const creature_mind::Random& random)
{
	std::vector<uint32_t> turns(actionCount);
	std::ranges::generate(turns, [&random] { return random(k_FreshTurnsRange); });
	return turns;
}

void creature_action_history::PlanEnded(std::span<uint32_t> turnsSinceDone, uint32_t action,
                                        std::span<const uint32_t> desireActions, uint32_t agendaTurns)
{
	if (action < turnsSinceDone.size())
	{
		turnsSinceDone[action] = 0;
	}
	for (const auto listed : desireActions.first(std::min(desireActions.size(), k_ListedActions)))
	{
		// The list ends at the first entry that is no action
		if (listed == 0 || listed >= turnsSinceDone.size())
		{
			break;
		}
		if (listed != action)
		{
			turnsSinceDone[listed] += agendaTurns;
		}
	}
}
