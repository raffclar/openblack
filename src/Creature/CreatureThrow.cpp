/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureThrow.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

using namespace openblack;
using namespace openblack::creature_throw;

namespace
{
constexpr float k_Tiny = 1e-4f;
} // namespace

float creature_throw::FlightTime(float distance)
{
	return std::sqrt(std::max(distance, 0.0f) / k_Gravity);
}

glm::vec3 creature_throw::ReleaseVelocity(const glm::vec3& target, const glm::vec3& release, float seconds)
{
	const auto time = std::max(seconds, k_Tiny);
	// Gravity takes half g t squared off the height on the way, which the throw makes up
	const glm::vec3 fall {0.0f, -0.5f * k_Gravity * time * time, 0.0f};
	return (target - release - fall) / time;
}

bool creature_throw::FarEnoughToThrow(float groundDistance, float size)
{
	return groundDistance >= size * k_HeightAtSizeOne * k_MinThrowHeightShare;
}

float creature_throw::HighThrowWeight(float targetSlope, float flatSlope, float highSlope)
{
	const auto span = highSlope - flatSlope;
	if (std::abs(span) < k_Tiny)
	{
		return 0.0f;
	}
	return std::clamp((targetSlope - flatSlope) / span, 0.0f, 1.0f);
}

glm::vec3 creature_throw::HandVelocity(const glm::vec3& atRelease, const glm::vec3& before, float spanMs)
{
	return spanMs > 0.0f ? (atRelease - before) * (1000.0f / spanMs) : glm::vec3(0.0f);
}

HeldPose creature_throw::HeldPlacement(const glm::mat3& hand, const glm::vec3& fingerEnds, bool mirrored, float hang)
{
	const auto side = glm::normalize(hand[0]);
	const auto ahead = glm::normalize(hand[1]);
	const auto up = glm::normalize(hand[2]);
	const auto heldUp = mirrored ? -up : up;
	const auto heldAhead = mirrored ? ahead : -ahead;
	return {.rotation = glm::mat3(-side, heldUp, heldAhead), .position = fingerEnds - heldUp * hang};
}

std::vector<uint32_t> creature_throw::TossPointBones(std::span<const uint32_t> parents, uint32_t hand)
{
	if (hand >= parents.size())
	{
		return {};
	}
	const auto count = static_cast<uint32_t>(parents.size());
	const auto hasChild = [&parents, count](uint32_t bone) {
		return std::ranges::any_of(parents, [bone](uint32_t parent) { return parent == bone; });
	};
	// The bones that start the branches: the hand, and those after it under its parent
	std::vector<uint32_t> open {hand};
	for (uint32_t bone = hand + 1; bone < count; ++bone)
	{
		if (parents[bone] == parents[hand])
		{
			open.push_back(bone);
		}
	}
	std::vector<uint32_t> ends;
	while (!open.empty())
	{
		const auto bone = open.back();
		open.pop_back();
		if (!hasChild(bone))
		{
			ends.push_back(bone);
			continue;
		}
		for (uint32_t child = 0; child < count; ++child)
		{
			if (parents[child] == bone)
			{
				open.push_back(child);
			}
		}
	}
	std::ranges::sort(ends);
	return ends;
}

glm::vec3 creature_throw::TossVelocity(const glm::vec3& handVelocity, bool mirrored, const glm::mat3& rotation, float share)
{
	auto local = handVelocity * share;
	if (mirrored)
	{
		local.x = -local.x;
	}
	return rotation * local;
}

std::optional<creature_throw::ThrowStand> creature_throw::WhereToThrowFrom(glm::vec2 creature, glm::vec2 target, float height,
                                                                           float targetRadius, float keep)
{
	const float distance = glm::distance(creature, target);
	const float nearest = keep + targetRadius + k_ThrowStandHeights * height;
	const float furthest = k_ThrowReachHeights * height;
	if (distance < nearest)
	{
		if (distance <= 0.0f)
		{
			return std::nullopt;
		}
		// Back along the line from the thing to where it stands, to the nearest it may stand
		return ThrowStand {.kind = ThrowStand::Kind::BackOff,
		                   .point = target - (target - creature) * (nearest / distance),
		                   .minDistance = 0.0f,
		                   .maxDistance = k_ThrowBackOffHeights * height};
	}
	if (distance > furthest)
	{
		return ThrowStand {.kind = ThrowStand::Kind::WalkUp,
		                   .point = target,
		                   .minDistance = nearest,
		                   .maxDistance = furthest > nearest ? furthest : nearest + k_ThrowWalkUpSpare};
	}
	return ThrowStand {};
}
