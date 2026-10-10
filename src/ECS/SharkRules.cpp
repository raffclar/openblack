/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SharkRules.h"

#include <cmath>

#include <numbers>

using namespace openblack;
using namespace openblack::ecs;

namespace
{
constexpr float k_WakeGrowth = 10.0f;
constexpr float k_WakeRate = 0.5f;
constexpr uint8_t k_WakeCell = 0x31;
constexpr uint32_t k_WakeColour = 0x90FFFFFFu;
} // namespace

float shark_rules::Heading(const glm::vec3& turnStart, const glm::vec3& position, float facing)
{
	if (position.x == turnStart.x && position.z == turnStart.z)
	{
		return facing;
	}
	const float angle = std::atan2(position.z - turnStart.z, position.x - turnStart.x);
	return angle < 0.0f ? angle + (2.0f * std::numbers::pi_v<float>) : angle;
}

glm::vec3 shark_rules::Drawn(const glm::vec3& turnStart, float startHeight, const glm::vec3& position, float endHeight,
                             float turnFraction)
{
	const float before = 1.0f - turnFraction;
	return {turnStart.x * before + position.x * turnFraction, startHeight * before + endHeight * turnFraction,
	        turnStart.z * before + position.z * turnFraction};
}

std::optional<water_rings::Ring> shark_rules::WakeRing(int32_t& timer, const glm::vec3& point, float heading,
                                                       int32_t frameMilliseconds)
{
	if (timer <= k_WakeInterval)
	{
		timer += frameMilliseconds;
		return std::nullopt;
	}
	timer = (timer % k_WakeInterval) + frameMilliseconds;
	return water_rings::Ring {.position = glm::vec3(point.x, 0.0f, point.z),
	                          .growth = k_WakeGrowth,
	                          .angle = heading,
	                          .rate = k_WakeRate,
	                          .cell = k_WakeCell,
	                          .argb = k_WakeColour};
}
