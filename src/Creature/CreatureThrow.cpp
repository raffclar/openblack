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

#include "3D/MapCoords.h"
#include "Common/ModelInverseSquareRoot.h"
#include "Common/X87.h"
#include "Common/X87Vector.h"

using namespace openblack;
using namespace openblack::creature_throw;

namespace
{
constexpr float k_Tiny = 1e-4f;
} // namespace

float creature_throw::FlightTime(const glm::vec3& creature, const glm::vec3& target)
{
	const float acrossX = creature.x - target.x;
	const float acrossZ = creature.z - target.z;
	const float distance = std::sqrt((acrossZ * acrossZ) + (acrossX * acrossX));
	return std::sqrt(distance * k_FlightTimePerUnit);
}

glm::vec3 creature_throw::PlaceIn(const ThrowTarget& target, float seconds, const MapAltitude& altitude)
{
	if (target.kind == ThrowTarget::Kind::Creature)
	{
		// A creature goes on along its heading at its speed. The ground comes from the map position of that place, and
		// the game aims at the full height of the creature.
		const float distance = target.speed * seconds;
		const float acrossX = x87::Mul(x87::Sin(target.heading), distance);
		const float acrossZ = -x87::Mul(x87::Cos(target.heading), distance);
		const float x = acrossX + target.position.x;
		const float z = acrossZ + target.position.z;
		const auto mapX = map_coords::FtoL((x * 65536.0f) * 0.1f);
		const auto mapZ = map_coords::FtoL((z * 65536.0f) * 0.1f);
		return {x, altitude(mapX, mapZ) + target.height, z};
	}
	// Anything else is aimed at the middle of its height over the ground where it stands
	glm::vec3 place {map_coords::ToMetres(target.mapPosition.x),
	                 altitude(target.mapPosition.x, target.mapPosition.y) + (target.height * 0.5f),
	                 map_coords::ToMetres(target.mapPosition.y)};
	if (target.kind == ThrowTarget::Kind::Villager && target.walking)
	{
		// A villager goes on by its step for each whole turn in the time. The height does not change.
		constexpr float k_MetresPerStepUnit = 1.0f / 65536.0f;
		const auto turns = static_cast<float>(map_coords::FtoL(static_cast<float>(k_LeadTurnsPerSecond) * seconds));
		const float stepX = (static_cast<float>(target.step.x) * 10.0f) * k_MetresPerStepUnit;
		const float stepZ = (static_cast<float>(target.step.y) * 10.0f) * k_MetresPerStepUnit;
		place.x = (turns * stepX) + place.x;
		place.z = (turns * stepZ) + place.z;
	}
	return place;
}

ThrowAim creature_throw::AimAt(const glm::vec3& creature, const ThrowTarget& target, const MapAltitude& altitude)
{
	const glm::vec3 standing {map_coords::ToMetres(target.mapPosition.x), 0.0f, map_coords::ToMetres(target.mapPosition.y)};
	const float first = FlightTime(creature, standing);
	const float second = FlightTime(creature, PlaceIn(target, first, altitude));
	return {.point = PlaceIn(target, second, altitude), .seconds = second};
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

HeldPose creature_throw::HeldPlacement(const glm::mat3& hand, const glm::vec3& fingerEnds, bool mirrored, float hang,
                                       float thingScale)
{
	// The squares of each axis are added from x to z
	const auto unit = [](const glm::vec3& axis) {
		return axis * gutils::ModelInverseSquareRoot(x87::Dot<x87::SumOrder::Xyz>(axis, axis));
	};
	const auto side = unit(hand[0]);
	const auto ahead = unit(hand[1]);
	const auto up = unit(hand[2]);
	const auto heldUp = mirrored ? -up : up;
	const auto heldAhead = mirrored ? ahead : -ahead;
	// The scaled up axis is made the hang distance long, its squares added from y to z, then x. An axis of no length
	// stays so.
	const auto scaledUp = heldUp * thingScale;
	const auto drop = scaledUp == glm::vec3(0.0f) ? scaledUp : x87::Scaled<x87::SumOrder::Yzx>(scaledUp, hang);
	return {.rotation = glm::mat3(-side, heldUp, heldAhead), .position = fingerEnds - drop};
}

HeldPose creature_throw::ReleaseStart(const HeldPose& held, const glm::vec3& fingerEnds, bool thrown)
{
	return {.rotation = held.rotation, .position = thrown ? fingerEnds : held.position};
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
	// The way from the creature to the thing, and its length with the square of z first
	const float acrossX = target.x - creature.x;
	const float acrossZ = target.y - creature.y;
	const float distance = std::sqrt((acrossZ * acrossZ) + (acrossX * acrossX));
	const float twoHeights = height + height;
	const float nearest = (keep + targetRadius) + twoHeights;
	const float furthest = height * k_ThrowReachHeights;
	if (distance < nearest)
	{
		if (distance <= 0.0f)
		{
			return std::nullopt;
		}
		// Back along the line from the thing to where it stands, to the nearest it may stand
		return ThrowStand {.kind = ThrowStand::Kind::BackOff,
		                   .point = {target.x - ((nearest * acrossX) / distance), target.y - ((nearest * acrossZ) / distance)},
		                   .minDistance = 0.0f,
		                   .maxDistance = twoHeights};
	}
	if (!(distance <= furthest))
	{
		return ThrowStand {.kind = ThrowStand::Kind::WalkUp,
		                   .point = target,
		                   .minDistance = nearest,
		                   .maxDistance = furthest > nearest ? furthest : nearest + k_ThrowWalkUpSpare};
	}
	return ThrowStand {};
}
