/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureMeals.h"

#include <algorithm>

using namespace openblack;

bool creature_meals::FieldHasFood(float cropAge, float cropFood)
{
	return cropAge > 0.0f && cropFood > 0.0f;
}

bool creature_meals::StoragePitServes(float pitRadius, float size, uint32_t pileAmount)
{
	return !(pitRadius > size * k_ThingsPerSize) && pileAmount != 0;
}

bool creature_meals::TreeBigEnough(float treeHeight, float size)
{
	return !(treeHeight < size * k_ThingsPerSize);
}

uint32_t creature_meals::HandfulAmount(float size, uint32_t thingHolds)
{
	const auto wanted = static_cast<uint32_t>(std::max(size * k_HandfulPerSize, 0.0f));
	return std::min(wanted, thingHolds);
}

float creature_meals::HungerAfterMeal(float hunger, float energy)
{
	return std::clamp(hunger - energy, 0.0f, 1.0f);
}

void creature_meals::CountMeal(Tallies& tallies, std::optional<uint32_t> sort, bool keptDown)
{
	if (!sort.has_value())
	{
		return;
	}
	CountKill(tallies, *sort);
	if (keptDown)
	{
		++tallies.keptDown[*sort];
	}
}

void creature_meals::CountKill(Tallies& tallies, uint32_t sort)
{
	switch (sort)
	{
	case k_CreatureSort:
		++tallies.creaturesKilled;
		break;
	case k_VillagerSort:
		++tallies.peopleKilled;
		break;
	case k_AnimalSort:
		++tallies.animalsKilled;
		break;
	default:
		break;
	}
}
