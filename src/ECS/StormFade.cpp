/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StormFade.h"

#include <algorithm>

#include "Common/X87.h"

namespace openblack::storm_fade
{

namespace
{
/// The share of the snow depth that the weather of a cell keeps
constexpr float k_SnowCoverShare = 0.5f;
/// The number of turns that a dead storm counts before it goes
constexpr uint8_t k_DeadStormTurns = 2;
} // namespace

bool DeadStormGoes(uint8_t& deadTurns)
{
	++deadTurns;
	return deadTurns >= k_DeadStormTurns;
}

Faded Fade(float age, float lastsFor, float fadeTime, float strength, float innerRadius)
{
	if (age < fadeTime)
	{
		return {.strength = age * strength / fadeTime, .innerRadius = age * innerRadius / fadeTime};
	}
	if (age > lastsFor - fadeTime)
	{
		const float left = lastsFor - age;
		return {.strength = left * strength / fadeTime, .innerRadius = left * innerRadius / fadeTime};
	}
	return {.strength = strength, .innerRadius = innerRadius};
}

int8_t SnowCover(float depth)
{
	return static_cast<int8_t>(std::clamp(x87::ToInteger(depth * k_SnowCoverShare), -128, 127));
}

} // namespace openblack::storm_fade
