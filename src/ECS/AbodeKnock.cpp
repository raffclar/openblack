/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AbodeKnock.h"

#include <algorithm>

namespace abode_knock = openblack::ecs::abode_knock;

namespace
{
/// The read-out's size goes in 255 steps, a step for every so many milliseconds of growing or shrinking
constexpr float k_StepsPerMs = 0.255f;
constexpr int32_t k_FullSteps = 255;
constexpr float k_StepShare = 1.0f / 255.0f;
} // namespace

uint32_t abode_knock::Knock(uint32_t remaining)
{
	if (remaining == 0)
	{
		return k_ReadoutMs;
	}
	if (remaining > k_ReadoutMs - k_ReadoutGrowMs)
	{
		// Still growing in
		return remaining;
	}
	if (remaining < k_ReadoutShrinkMs)
	{
		// Shrinking away: grow back from the size it has shrunk to
		return k_ReadoutMs - remaining;
	}
	return k_ReadoutMs - k_ReadoutGrowMs;
}

float abode_knock::ReadoutScale(uint32_t remaining)
{
	int32_t steps = k_FullSteps;
	// The game works the steps out at a higher precision than a float and cuts off the fraction
	if (remaining > k_ReadoutMs - k_ReadoutGrowMs)
	{
		const double grown = static_cast<double>(remaining - (k_ReadoutMs - k_ReadoutGrowMs)) * k_StepsPerMs;
		steps = static_cast<int32_t>(static_cast<double>(k_FullSteps) - grown);
	}
	else if (remaining <= k_ReadoutShrinkMs)
	{
		steps = static_cast<int32_t>(static_cast<double>(remaining) * k_StepsPerMs);
	}
	return static_cast<float>(static_cast<double>(steps) * k_StepShare);
}

std::vector<abode_knock::Marker> abode_knock::Readout(uint32_t places, uint32_t adults, float scale)
{
	std::vector<Marker> markers;
	markers.reserve(places);
	const float start = -static_cast<float>(places) * 0.5f;
	const float size = std::max(scale * k_MarkerSpacing, k_SmallestMarker);
	for (uint32_t i = 0; i < places; ++i)
	{
		markers.push_back({
		    .offset = start + static_cast<float>(i) * k_MarkerSpacing,
		    .colour = i < adults ? k_LivedInColour : k_FreePlaceColour,
		    .size = size,
		});
	}
	return markers;
}
