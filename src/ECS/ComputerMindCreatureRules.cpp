/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerMindCreatureRules.h"

#include <cmath>

#include <algorithm>

using namespace openblack;

float computer_mind_rules::CreatureFreedom(const CreatureFreedomFacts& facts)
{
	if (!facts.leashed)
	{
		return 1.0f;
	}
	const auto turns = static_cast<float>(facts.turnsSinceTied);
	if (!(turns > k_LeashBusyTurns) || facts.fighting)
	{
		return 0.0f;
	}
	const float time = (turns - k_LeashBusyTurns) * k_LeashFreedomPerTurn;
	const float life = facts.life > k_FitShare ? 1.0f : facts.life;
	const float rest = facts.exhaustion >= k_FitShare ? 1.0f - facts.exhaustion : 1.0f;
	const float energy = facts.energy > k_FitShare ? 1.0f : facts.energy;
	const float fitness = facts.restoringHealth ? 0.0f : life * (energy * rest);
	const float freedom = fitness * time;
	return freedom >= 1.0f ? 1.0f : freedom;
}

float computer_mind_rules::StarvingWish(float hunger)
{
	return hunger < 1.0f ? hunger : 1.0f;
}

int32_t computer_mind_rules::StarvingAmount(float wish)
{
	return static_cast<int32_t>(wish * 1000.0f);
}

bool computer_mind_rules::LongJourney(glm::vec3 creature, glm::vec3 place)
{
	const float dx = place.x - creature.x;
	const float dy = place.y - creature.y;
	const float dz = place.z - creature.z;
	return std::sqrt(dz * dz + dy * dy + dx * dx) > k_LongJourney;
}

float computer_mind_rules::JourneySpeed(bool longJourney, float walk, float run)
{
	const float speed = longJourney ? run * k_LongJourneyRun : (run + walk) * 0.5f;
	// The creature keeps a speed from none to its top speed
	if (!(speed > 0.0f))
	{
		return 0.0f;
	}
	return speed < 1.0f ? speed : 1.0f;
}
