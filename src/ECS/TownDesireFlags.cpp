/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownDesireFlags.h"

#include <cmath>

#include <algorithm>
#include <numbers>

namespace openblack::ecs::town_desire_flags
{

std::optional<uint32_t> PointOf(int32_t place)
{
	if (place < 0)
	{
		return std::nullopt;
	}
	return k_FirstFlagPoint + static_cast<uint32_t>(place);
}

bool Shows(bool storeWorking, float level, float showsAfter)
{
	return storeWorking && showsAfter <= level;
}

field_crop::Settle Step(field_crop::Settle level, bool shown, float desire, float seconds)
{
	if (!shown)
	{
		return {.position = desire, .speed = 0.0f};
	}
	return field_crop::Advance(level, std::min(desire, 1.0f), seconds);
}

float Rise(float level)
{
	return level * k_RiseHeight;
}

float DrawnScale(float level, float showsAfter)
{
	const float whole = (1.0f - showsAfter) * k_GrowShare;
	if (whole <= level)
	{
		return 1.0f;
	}
	return (level - showsAfter) / (whole - showsAfter);
}

float FacingAngle(glm::vec3 flag, glm::vec3 camera)
{
	return std::atan2(camera.z - flag.z, camera.x - flag.x) + (std::numbers::pi_v<float> / 2.0f);
}

bool InView(glm::vec4 clip)
{
	return clip.w > 0.0f && std::abs(clip.x) <= clip.w && std::abs(clip.y) <= clip.w;
}

std::optional<uint32_t> ToolTipWords(TownDesireInfo desire)
{
	// The words follow on from the tooltip "Food Desire", one for each desire in turn
	constexpr uint32_t k_FoodDesireWords = 91;
	switch (desire)
	{
	case TownDesireInfo::ForFood:
	case TownDesireInfo::ForWood:
	case TownDesireInfo::ForProtection:
	case TownDesireInfo::ForMercy:
	case TownDesireInfo::ForAbodes:
	case TownDesireInfo::ForCivicBuilding:
	case TownDesireInfo::ForChildren:
		return k_FoodDesireWords + static_cast<uint32_t>(desire);
	default:
		return std::nullopt;
	}
}

float NeedPercent(float desire)
{
	return std::min(desire, 1.0f) * 100.0f;
}

} // namespace openblack::ecs::town_desire_flags
