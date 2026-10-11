/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BigForestRules.h"

#include <cmath>

using namespace openblack;

float big_forest::StartWood(float scale, uint32_t kindWood)
{
	return scale * static_cast<float>(kindWood);
}

float big_forest::WoodValue(float life, float wood)
{
	return life * wood;
}

big_forest::WoodTaken big_forest::TakeWood(float life, float wood, float scale, uint32_t kindWood, uint32_t amount)
{
	// What is taken comes off what it holds divided by its life
	const float share = static_cast<float>(amount) / life;
	const float worth = WoodValue(life, wood);
	if (worth <= share)
	{
		// Used up: all it was worth is taken, cut to whole units
		return {.woodLeft = 0.0f, .usedUp = true, .shrinksTo = std::nullopt, .taken = static_cast<uint32_t>(worth)};
	}
	WoodTaken result {.woodLeft = wood - share, .usedUp = false, .shrinksTo = std::nullopt, .taken = amount};
	const float drawn = life * scale * static_cast<float>(kindWood);
	const float left = WoodValue(life, result.woodLeft);
	if (std::abs(left - drawn) > k_ShrinkDrift)
	{
		result.shrinksTo = left / (life * static_cast<float>(kindWood));
	}
	return result;
}

bool big_forest::BlocksPine(float distance, float radius)
{
	return distance + radius < k_PineClearance;
}
