/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerAge.h"

#include "ECS/Components/Villager.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;

float villager_age::StartScale(uint32_t age, uint32_t grownUp, std::span<const float, 20> ageToScale)
{
	if (IsChildAge(age, grownUp) && age < ageToScale.size())
	{
		return ageToScale[age];
	}
	return k_AdultStartScale;
}

float villager_age::GrownScale(uint32_t age, uint32_t grownUp, std::span<const float, 20> ageToScale, float scale,
                               const FloatRandom& random)
{
	if (IsChildAge(age, grownUp) && age < ageToScale.size())
	{
		const float gap = ageToScale[age] - scale;
		const float most = gap * k_ChildGrowthShare;
		const float growth = random(most);
		return scale + growth;
	}
	const float first = k_AdultLargestScale - random(k_AdultScaleRange);
	if (first <= scale)
	{
		return scale;
	}
	// The game draws again rather than keeping the size it compared
	return k_AdultLargestScale - random(k_AdultScaleRange);
}

bool villager_age::DiesOfOldAge(uint32_t age, const Ages& ages, const FloatRandom& floatRandom, const IntRandom& intRandom)
{
	if (age <= ages.old)
	{
		return false;
	}
	const uint32_t span = ages.oldest - ages.old;
	const float draw = floatRandom(1.0f);
	float cubed = draw;
	cubed *= draw;
	cubed *= draw;
	const float most = cubed * static_cast<float>(span);
	const auto extra = intRandom(static_cast<uint32_t>(most));
	return age + extra > ages.oldest;
}

namespace
{
uint32_t TurnNow()
{
	return Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
}
} // namespace

uint32_t villager_age::AgeNow(const components::Villager& villager)
{
	return AgeOf(TurnNow(), villager.birthTurn);
}

void villager_age::SetBirthTurnForAge(components::Villager& villager, uint32_t age)
{
	villager.birthTurn = BirthTurnFor(TurnNow(), age);
}
