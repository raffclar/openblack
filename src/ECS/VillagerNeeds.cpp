/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerNeeds.h"

#include <algorithm>

using namespace openblack::ecs;

float villager_needs::DesireForFood(float food)
{
	const float full = std::min(food, 1.0f);
	// Multiplied in turn, as the game does
	float cubed = full;
	cubed *= full;
	cubed *= full;
	return 1.0f - cubed;
}

float villager_needs::LifeDesireFromLife(float life, float goHomeLife)
{
	const float least = std::min(goHomeLife, life);
	const float share = (life - least) / (1.0f - goHomeLife);
	return 1.0f - (share * share);
}

float villager_needs::OwnDesiresTrigger(const TriggerInputs& in)
{
	if (in.woken)
	{
		return 0.0f;
	}
	const float food = in.hungry ? in.foodDesire : 0.0f;
	const float pastThreshold = in.lifeDesire - in.ownDesireThreshold;
	const float life = pastThreshold > 0.0f ? pastThreshold : 0.0f;
	const float trigger = (std::min(food, life) * k_LesserNeedShare) + std::max(food, life);
	if (in.child && trigger <= k_ChildLeastTrigger)
	{
		return k_ChildLeastTrigger;
	}
	return std::min(trigger, 1.0f);
}

bool villager_needs::SatisfyOwnDesire(float foodDesire, float lifeDesire, float threshold, const std::function<bool()>& sleep,
                                      const std::function<bool()>& eat)
{
	const float food = foodDesire - threshold;
	const float life = lifeDesire - threshold;
	if (food > life && food > 0.0f)
	{
		if (eat())
		{
			return true;
		}
		return life > 0.0f && sleep();
	}
	if (life > 0.0f)
	{
		if (sleep())
		{
			return true;
		}
		return food > 0.0f && eat();
	}
	return false;
}

bool villager_needs::PausesForASecond(float draw, float life, bool poisoned, float pauseChance)
{
	const float felt = poisoned ? life * k_PoisonedLifeShare : life;
	const float missing = 1.0f - felt;
	const float weight = missing * missing * missing;
	return draw - (k_PauseLifeWeight * weight) < pauseChance;
}
