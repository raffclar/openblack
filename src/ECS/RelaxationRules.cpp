/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "RelaxationRules.h"

#include <algorithm>

namespace rules = openblack::ecs::relaxation_rules;

namespace
{
constexpr uint32_t k_FullTeam = 4;
} // namespace

float rules::FootballDesire(uint32_t age, const FootballGame& game, uint32_t now,
                            const std::function<float(float)>& gameFloatRand)
{
	if (age < k_YoungestFootballer)
	{
		return 0.0f;
	}
	if (game.firstTeam > k_FullTeam && game.secondTeam > k_FullTeam && game.playing)
	{
		return 0.0f;
	}
	if (game.lastGameTurn == 0)
	{
		return 1.0f;
	}
	// Whole rest periods only: under one gives a share of nothing
	const uint32_t since = std::min(now - game.lastGameTurn, k_FootballRestTurns);
	const auto periods = static_cast<float>(since / k_FootballRestTurns);
	return gameFloatRand ? gameFloatRand(periods) : 0.0f;
}

rules::Chosen rules::Choose(std::optional<float> football, std::span<const float> artefacts)
{
	float best = 0.0f;
	Chosen chosen;
	if (football.has_value())
	{
		best = *football;
		chosen.choice = Choice::Football;
	}
	for (size_t i = 0; i < artefacts.size(); ++i)
	{
		if (artefacts[i] > best)
		{
			best = artefacts[i];
			chosen = {.choice = Choice::Artefact, .index = i};
		}
	}
	if (best == 0.0f)
	{
		return {};
	}
	return chosen;
}
