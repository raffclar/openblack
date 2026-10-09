/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFireReaction.h"

using namespace openblack;
using namespace openblack::creature_fire;

uint8_t creature_fire::Priority(float distance, float targetRadius, float selfRadius, float selfHeight, float targetHeight,
                                uint8_t firePriority)
{
	// Both tests are strict, and worked without rounding the sum or the share to a float, as the game compares them
	if (static_cast<double>(distance) < static_cast<double>(targetRadius) + static_cast<double>(selfRadius) &&
	    static_cast<double>(selfHeight) / static_cast<double>(targetHeight) > static_cast<double>(k_LeastHeightShare))
	{
		return firePriority;
	}
	return 0;
}

Response creature_fire::Choose(std::optional<float> usefulness, float compassion, float seen, float needed)
{
	if (!usefulness.has_value() ||
	    !(static_cast<double>(*usefulness) * static_cast<double>(compassion) > static_cast<double>(k_LeastCompassionateUse)))
	{
		return Response::RunAway;
	}
	// Having seen the water miracle often enough means a share of at least one; a share that is no number at all (never
	// seen, and nothing needed) counts as enough, as the game's comparison lets it through
	const auto share = static_cast<double>(seen) / static_cast<double>(needed);
	return share < 1.0 ? Response::RunAway : Response::PutOut;
}

BelongsTo creature_fire::Owner(Burning burning)
{
	switch (burning)
	{
	case Burning::Building:
	case Burning::Villager:
	case Burning::Field:
	case Burning::TotemStatue:
		return BelongsTo::Town;
	case Burning::Tree:
		return BelongsTo::Forest;
	case Burning::Animal:
		return BelongsTo::Flock;
	case Burning::Creature:
		return BelongsTo::Itself;
	case Burning::TemplePart:
		return BelongsTo::Temple;
	case Burning::Other:
		break;
	}
	return BelongsTo::Nothing;
}
