/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownDesireAlignment.h"

namespace openblack::ecs::town_desire_alignment
{

float Change(std::array<uint32_t, components::k_TownDesireCount>& turnsOver, const Wants& wants,
             std::span<const DesireRule, components::k_TownDesireCount> rules, People people, float maxChangePerTurn)
{
	float total = 0.0f;
	for (size_t d = 0; d < components::k_TownDesireCount; ++d)
	{
		// The want with its boosts, added in the game's order
		const float want = (wants.boostA[d] + wants.desire[d]) + wants.boost[d];
		const auto& rule = rules[d];
		auto& turns = turnsOver.at(d);
		if (!(want > rule.affectsAfter))
		{
			if (turns != 0)
			{
				--turns;
			}
			continue;
		}
		++turns;
		// The share of the most turns, no more than all of them, cubed
		float share = static_cast<float>(turns) / static_cast<float>(rule.maxTurns);
		if (share > 1.0f)
		{
			share = 1.0f;
		}
		const float cube = (share * share) * share;
		if (people.all == 0)
		{
			continue;
		}
		const float townShare = static_cast<float>(people.town) / static_cast<float>(people.all);
		const float change = ((cube * rule.importance) * townShare) * want;
		total = change + total;
	}
	if (total < -1.0f)
	{
		total = -1.0f;
	}
	else if (total > 1.0f)
	{
		total = 1.0f;
	}
	return total * maxChangePerTurn;
}

} // namespace openblack::ecs::town_desire_alignment
