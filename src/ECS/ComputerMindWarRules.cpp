/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerMindWarRules.h"

using namespace openblack;

float computer_mind_rules::AttackCreatureWish(float distance, float nearness)
{
	// The game finds the nearness first, then puts 1 in its place when the creature is near
	return distance < k_AttackCreatureNear ? 1.0f : nearness;
}

float computer_mind_rules::BuildingTargetValue(float built, float life, float attackDesire)
{
	// The order of the products is the order of the game
	const float standing = life * built;
	return standing * attackDesire;
}

float computer_mind_rules::FieldTargetValue(float food, float totalFood, float attackDesire)
{
	const float share = food / totalFood;
	return share * attackDesire;
}

float computer_mind_rules::NearTargetValue(float value, float nearness)
{
	return nearness * value;
}

std::optional<size_t> computer_mind_rules::MostWantedTarget(std::span<const float> values)
{
	std::optional<size_t> best;
	float most = 0.0f;
	for (size_t i = 0; i < values.size(); ++i)
	{
		if (values[i] > most)
		{
			most = values[i];
			best = i;
		}
	}
	return best;
}

float computer_mind_rules::RockScore(const RockFacts& facts)
{
	if (!facts.throwable || !(facts.destroysBuildings || facts.tree))
	{
		return 0.0f;
	}
	return facts.weight;
}
