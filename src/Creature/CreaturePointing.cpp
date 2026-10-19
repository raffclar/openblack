/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreaturePointing.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include <glm/geometric.hpp>

#include "Common/X87.h"
#include "Common/X87Angle.h"
#include "Common/X87Vector.h"
#include "Creature/CreatureWalkPlanner.h"

using namespace openblack;

float creature_pointing::AngleOf(glm::vec2 direction)
{
	return x87::RoundToFloat(x87::GameHeadingOf(direction.x, direction.y));
}

float creature_pointing::Wrap(float angle)
{
	constexpr float k_HalfTurn = std::numbers::pi_v<float>;
	constexpr float k_FullTurn = 2.0f * std::numbers::pi_v<float>;
	if (angle > k_HalfTurn)
	{
		return angle - k_FullTurn;
	}
	if (angle < -k_HalfTurn)
	{
		return angle + k_FullTurn;
	}
	return angle;
}

creature_pointing::Aim creature_pointing::AimAt(glm::vec3 origin, glm::vec2 facing, glm::vec3 target)
{
	return AimAt(origin, GameHeading(AngleOf(facing)), target);
}

creature_pointing::Aim creature_pointing::AimAt(glm::vec3 origin, GameHeading heading, glm::vec3 target)
{
	// The game adds the squares z, x, y, and keeps the heading to the target wide into the subtraction
	const auto offset = target - origin;
	const auto distance = x87::Length<x87::SumOrder::Zxy>(offset);
	if (distance < k_TooNear)
	{
		return {};
	}
	const auto across = Wrap(x87::Sub(heading.Radians(), x87::GameHeadingOf(offset.x, offset.z)));
	return {.across = std::clamp(across, -k_AcrossLimit, k_AcrossLimit),
	        .rise = std::clamp(offset.y / distance, -k_RiseLimit, k_RiseLimit)};
}

float creature_pointing::TurnAngle(glm::vec3 head, glm::vec2 facing, glm::vec3 target)
{
	return TurnAngle(head, GameHeading(AngleOf(facing)), target);
}

float creature_pointing::TurnAngle(glm::vec3 head, GameHeading heading, glm::vec3 target)
{
	// The heading to the target stays wide into the subtraction
	return Wrap(x87::Sub(x87::GameHeadingOf(target.x - head.x, target.z - head.z), heading.Radians()));
}

bool creature_pointing::PointsWithLeft(float turnAngle, std::optional<bool> heldInLeft)
{
	if (heldInLeft.has_value())
	{
		return !*heldInLeft;
	}
	return turnAngle > 0.0f;
}

bool creature_pointing::PointsWithLeftAfterTurn(const Aim& aim, std::optional<bool> heldInLeft)
{
	if (heldInLeft.has_value())
	{
		return !*heldInLeft;
	}
	return aim.across > 0.0f;
}

std::array<float, 4> creature_pointing::Weights(const Aim& aim, bool left)
{
	const auto across = left ? -aim.across : aim.across;
	const auto right = (across + k_AcrossLimit) * k_AcrossScale;
	const auto high = aim.rise + k_RiseLimit;
	return {(1.0f - right) * (1.0f - high), right * (1.0f - high), (1.0f - right) * high, right * high};
}

float creature_pointing::RoutePointTime(float timeMs, int32_t stepMs, float pointMs, bool ending, int32_t durationMs)
{
	if (!ending && timeMs >= pointMs)
	{
		return timeMs;
	}
	return static_cast<float>(creature_walk_planner::AdvanceLoop(static_cast<int32_t>(timeMs), stepMs, durationMs));
}

bool creature_pointing::RoutePointOver(bool ready, bool ending, float timeMs, int32_t stepMs, float endDurationMs)
{
	return ready && ending && static_cast<int32_t>(timeMs) + stepMs >= static_cast<int32_t>(endDurationMs);
}

float creature_pointing::AdvanceTime(float timeMs, float stepMs, float pointMs, bool ending)
{
	if (ending || timeMs < pointMs)
	{
		return timeMs + stepMs;
	}
	return timeMs;
}
