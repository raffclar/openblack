/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DesignedWaterfall.h"

#include "Common/X87.h"

namespace openblack::designed_waterfall
{

namespace
{
constexpr float k_SecondsPerMillisecond = 0.001f;
/// The water's texture slides by this part of its height each second
constexpr float k_SlidePerSecond = 0.5f;
/// The ring of spray
constexpr glm::vec3 k_SprayPosition {3018.8f, 0.2f, 3130.15f};
constexpr float k_SprayGrowth = 30.0f;
constexpr float k_SprayRate = 0.3f;
constexpr uint8_t k_SprayCell = 0x30;
constexpr uint32_t k_SprayAlpha = 0x80u;
} // namespace

Changes Arrive(Presence& presence, int32_t land, bool modelShown)
{
	Changes changes;
	if (presence.land != land)
	{
		changes.removeModel = presence.model;
		changes.removeSound = presence.sound;
		presence = {.land = land, .model = false, .sound = false};
	}
	if (land != k_Land)
	{
		return changes;
	}
	if (!presence.model)
	{
		changes.makeModel = true;
		changes.makeSound = true;
		presence.model = true;
		presence.sound = true;
	}
	else if (!modelShown)
	{
		changes.makeModel = true;
	}
	return changes;
}

void LandCleared(Presence& presence)
{
	presence.sound = false;
}

float Slide(float slide, int32_t milliseconds)
{
	const float seconds = x87::Mul(static_cast<double>(milliseconds), k_SecondsPerMillisecond);
	const float moved = x87::Sub(slide, x87::Mul(seconds, k_SlidePerSecond));
	// The whole part goes towards zero, so the slide stays above -1
	return moved - static_cast<float>(x87::ToInteger(moved));
}

bool StepSpray(float& clock, int32_t milliseconds)
{
	clock = x87::Add(clock, x87::Mul(static_cast<double>(milliseconds), k_SecondsPerMillisecond));
	if (!(clock > k_SprayInterval))
	{
		return false;
	}
	clock = 0.0f;
	return true;
}

water_rings::Ring Spray(uint32_t landLight)
{
	return {
	    .position = k_SprayPosition,
	    .growth = k_SprayGrowth,
	    .angle = 0.0f,
	    .aspect = 1.0f,
	    .rate = k_SprayRate,
	    .cell = k_SprayCell,
	    .argb = (k_SprayAlpha << 24u) | (landLight & 0xFFFFFFu),
	};
}

} // namespace openblack::designed_waterfall
