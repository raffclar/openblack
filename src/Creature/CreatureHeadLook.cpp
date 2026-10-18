/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHeadLook.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "Common/X87.h"
#include "Common/X87Vector.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"

using namespace openblack;
using namespace openblack::creature_head_look;

namespace
{
/// A direction this near to a map axis gives no sideways turn. A head this near to straight plays no head clip.
constexpr float k_Hair = 1e-4f;
} // namespace

bool creature_head_look::AllowsLook(Pose pose, int32_t pointTimeMs, int32_t pointDurationMs)
{
	switch (pose)
	{
	case Pose::Standing:
	case Pose::Moving:
	case Pose::TurningToFace:
	case Pose::Sitting:
	case Pose::WaitingToCatch:
		return true;
	case Pose::Pointing:
		// The point holds the head straight from a fifth of the clip to seven tenths, both in whole milliseconds
		return pointTimeMs > (pointDurationMs * 7) / 10 || pointTimeMs < (pointDurationMs * 2) / 10;
	case Pose::TurningToWalk:
	case Pose::SittingDown:
	case Pose::GettingUp:
	case Pose::FightStance:
	case Pose::Other:
		return false;
	}
	return false;
}

Aim creature_head_look::AimOf(const AimFacts& facts)
{
	const bool catching = facts.pose == Pose::WaitingToCatch && facts.hasThing;
	const bool hasLookPoint = facts.hasLookPoint || catching;
	const bool stance = facts.pose == Pose::FightStance && facts.hasOpponent;
	if (!(hasLookPoint && AllowsLook(facts.pose, facts.pointTimeMs, facts.pointDurationMs)) && !stance)
	{
		return Aim::Ahead;
	}
	if (stance)
	{
		return Aim::OpponentHead;
	}
	return catching ? Aim::Thing : Aim::LookPoint;
}

Angles creature_head_look::AnglesTowards(const glm::vec3& eyes, GameHeading facing, const glm::vec3& target)
{
	auto direction = target - eyes;
	// The length adds the squares of y, z, then x. A direction of no length stays as it is.
	if (direction != glm::vec3(0.0f))
	{
		direction = x87::Scaled<x87::SumOrder::Yzx>(direction);
	}
	Angles angles {};
	// A target straight along a map axis gives no sideways turn
	if (std::abs(direction.x) > k_Hair && std::abs(direction.z) > k_Hair)
	{
		angles.yaw = creature_locomotion::HeadingOf(glm::vec2(direction.x, direction.z)).TurnFrom(facing);
	}
	angles.pitch = x87::RoundToFloat(x87::Asin(direction.y));
	return angles;
}

float creature_head_look::Acceleration(float size, float speciesRate)
{
	return creature_layers::PlaybackRate(size) * speciesRate;
}

bool creature_head_look::UsesSittingClips(Pose pose)
{
	return pose == Pose::SittingDown || pose == Pose::Sitting || pose == Pose::GettingUp;
}

bool creature_head_look::PlaysHeadClip(float angle, bool hasLookPoint)
{
	return std::abs(angle) > k_Hair || hasLookPoint;
}

RouteLook creature_head_look::StepRouteLook(RouteLook look, int32_t stepMs, const RouteLookFacts& facts,
                                            const std::function<float(glm::vec2)>& groundAt)
{
	look.countdownMs -= stepMs;
	if (look.countdownMs >= 0)
	{
		return look;
	}
	const auto at = facts.searchHead.value_or(glm::vec2(facts.destination.x, facts.destination.x));
	look.point = glm::vec3(at.x, groundAt(at), at.y);
	look.countdownMs = k_RouteLookMs;
	return look;
}
