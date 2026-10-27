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

glm::vec3 computer_mind_rules::ThrowFoodHandPlace(glm::vec3 hand, glm::vec3 creature)
{
	const float dx = creature.x - hand.x;
	const float dy = creature.y - hand.y;
	const float dz = creature.z - hand.z;
	const float zz = dz * dz;
	const float xx = dx * dx;
	const float yy = dy * dy;
	const float distance = std::sqrt((yy + xx) + zz);
	// The game takes a side that is not a number as 0
	const auto zero = [](float side) { return !(side < 0.0f) && !(side > 0.0f); };
	// The direction from the hand to the creature has the length 1, unless the hand is at the place of the creature
	glm::vec3 direction {dx, dy, dz};
	if (!zero(dx) || !zero(dy) || !zero(dz))
	{
		const float inverse = 1.0f / std::sqrt(zz + (yy + xx));
		direction = {dx * inverse, dy * inverse, dz * inverse};
	}
	if (!(distance >= k_ThrowFoodDistance))
	{
		return {hand.x - direction.x * k_ThrowFoodDistance, hand.y - direction.y * k_ThrowFoodDistance,
		        hand.z - direction.z * k_ThrowFoodDistance};
	}
	const float along = distance - k_ThrowFoodDistance;
	return {direction.x * along + hand.x, direction.y * along + hand.y, direction.z * along + hand.z};
}

glm::vec3 computer_mind_rules::ThrowFoodAim(glm::vec3 creature, float size, float firstDraw, float secondDraw)
{
	constexpr float k_Half = k_ThrowFoodScatter * 0.5f;
	return {(firstDraw - k_Half) + creature.x, size * k_ThrowFoodHeightPerSize, (secondDraw - k_Half) + creature.z};
}

uint32_t computer_mind_rules::FeedbackTurns(float draw)
{
	const float turns = (draw + k_FeedbackLeastSeconds) * 10.0f;
	return static_cast<uint32_t>(static_cast<int32_t>(turns));
}

float computer_mind_rules::CastLetGoWish(float freedom, float seen, float needed)
{
	const float part = seen / needed;
	const float learnt = part > 1.0f ? 1.0f : part;
	return freedom * (1.0f - learnt);
}

bool computer_mind_rules::LetsGoAtCast(float freedom, float wish, float distance, float size)
{
	// A wish that is not a number makes the compare fail, and the god continues to let go
	if (freedom >= wish)
	{
		return false;
	}
	const float near = size * k_ThrowFoodHeightPerSize * k_LetGoHeights + k_LetGoDistance;
	return near > distance;
}

bool computer_mind_rules::UntiesEvilLeash(float influence, bool restoringHealth)
{
	return !(influence > 0.0f) || restoringHealth;
}
