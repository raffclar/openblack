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

float BeamShare(float clock)
{
	const float share = (clock - k_BeamsFrom) / k_BeamsOver;
	if (share <= 0.0f)
	{
		return 0.0f;
	}
	return share >= 1.0f ? 1.0f : share;
}

float NextBeam(float beamClock, float share)
{
	// Each step rounds to a float: the gap goes from the first to the last as the share grows, then the clock is added
	const float gap = (k_LastBeamGap - k_FirstBeamGap) * share + k_FirstBeamGap;
	return gap + beamClock;
}

BeamLook BeamLookAt(float share)
{
	// Each step rounds to a float
	const auto between = [share](float first, float last) { return (last - first) * share + first; };
	return {
	    .life = between(k_FirstBeamLife, k_LastBeamLife),
	    .speed = between(k_FirstBeamSpeed, k_LastBeamSpeed),
	    .alpha = static_cast<uint8_t>(static_cast<int32_t>(between(k_FirstBeamAlpha, k_LastBeamAlpha))),
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
	// The share and the scale come together first, then the turns in a second multiply them
	const float seconds = (share + k_SmokeShareFrom) * k_SmokeSecondsScale;
	return static_cast<int32_t>(static_cast<float>(1000u / millisecondsPerTurn) * seconds);
}

} // namespace openblack::temple_destruction
