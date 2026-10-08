/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleDestruction.h"

#include <algorithm>

namespace openblack::temple_destruction
{

Events Between(float before, float after)
{
	return {
	    .loopStarts = Passes(before, k_LoopFrom, after),
	    .glow = Passes(before, k_GlowAt, after),
	    .explosion = Passes(before, k_ExplodeAt, after),
	    .smoke = Passes(before, k_EndAt, after),
	    .end = Passes(before, k_EndAt, after),
	};
}

uint8_t HeartAlpha(float clock)
{
	constexpr float k_Whole = 255.0f;
	const float faded = std::clamp((clock - k_FadeFrom) / k_FadeOver, 0.0f, 1.0f);
	return static_cast<uint8_t>(static_cast<int32_t>(k_Whole - (k_Whole * faded)));
}

int32_t TurnsFor(uint32_t millisecondsPerTurn, float seconds)
{
	return static_cast<int32_t>(static_cast<float>(1000u / millisecondsPerTurn) * seconds);
}

int32_t SmokeTurns(uint32_t millisecondsPerTurn, float share)
{
	return static_cast<int32_t>(static_cast<float>(1000u / millisecondsPerTurn) * (k_SmokeShareFrom + share) *
	                            k_SmokeSecondsScale);
}

} // namespace openblack::temple_destruction
