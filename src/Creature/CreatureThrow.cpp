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
	const float fall = ((time * time) * 0.5f) * -k_Gravity;
	const float inverse = 1.0f / time;
	return {(target.x - release.x) * inverse, ((target.y - release.y) - fall) * inverse, (target.z - release.z) * inverse};
}

bool creature_throw::FarEnoughToThrow(float groundDistance, float size)
{
	return groundDistance >= size * k_HeightAtSizeOne * k_MinThrowHeightShare;
}

float creature_throw::HighThrowWeight(float targetSlope, float flatSlope, float highSlope)
{
	return (targetSlope - flatSlope) / (highSlope - flatSlope);
}

float creature_throw::HurlHandSpeed(const glm::vec3& atThrow, const glm::vec3& before)
{
	constexpr float k_PerSecond = 10.0f;
	// The game keeps this factor as a double
	constexpr double k_Share = 0.707;
	const float up = atThrow.y - before.y;
	const float ahead = atThrow.z - before.z;
	return x87::Mul(std::sqrt((ahead * ahead) + (up * up)) * k_PerSecond, k_Share);
}

glm::vec3 creature_throw::HurlReleasePoint(const HurlHands& hands, const glm::vec3& position, GameHeading heading, float scale,
                                           bool mirrored, float slope)
{
	const float flatSlope = hands.FlatSlope();
	const float high = (slope - flatSlope) / (hands.HighSlope() - flatSlope);
	const float flat = 1.0f - high;
	const float x = (high * hands.high.x) + (flat * hands.flat.x);
	const float y = (high * hands.high.y) + (flat * hands.flat.y);
	const float z = (high * hands.high.z) + (flat * hands.flat.z);
	const float across = (mirrored ? -x : x) * scale;
	const float ahead = z * scale;
	const float cosine = x87::RoundToFloat(x87::Cos(heading.Radians()));
	const float sine = x87::RoundToFloat(x87::Sin(heading.Radians()));
	// The game adds 0 to the turned x and z before it adds the position
	return {(((cosine * across) + (-sine * ahead)) + 0.0f) + position.x, (y * scale) + position.y,
	        (((cosine * ahead) + (sine * across)) + 0.0f) + position.z};
}

creature_throw::HurlLift creature_throw::LiftOf(const HurlHands& hands, const glm::vec3& position, float scale, bool mirrored,
                                                const glm::vec3& target, float seconds)
{
	const auto heading = GameHeading::Of(target.x - position.x, target.z - position.z);
	const auto release = HurlReleasePoint(hands, position, heading, scale, mirrored, k_HurlReleaseSlope);
	const float fall = ((seconds * seconds) * 0.5f) * -k_Gravity;
	const float inverse = 1.0f / seconds;
	const float x = ((target.x - release.x) - 0.0f) * inverse;
	const float y = ((target.y - release.y) - fall) * inverse;
	const float z = ((target.z - release.z) - 0.0f) * inverse;
	const float speed = std::sqrt((x * x) + (z * z));
	// The height speed is kept between minus the speed and the speed. Each compare with no order (a value that is not a
	// number) takes the limit.
	float rise = y;
	if (!(rise >= -speed))
	{
		rise = -speed;
	}
	if (!(speed >= rise))
	{
		rise = speed;
	}
	return {.speed = speed, .rise = rise};
}

float creature_throw::HurlHighWeight(const HurlHands& hands, const HurlLift& lift)
{
	return HighThrowWeight(lift.rise / lift.speed, hands.FlatSlope(), hands.HighSlope());
}

int32_t creature_throw::HurlStepMs(const HurlHands& hands, const HurlLift& lift, float highWeight, float scale, int32_t turnMs,
                                   int32_t clipStepMs, int32_t timeMs, int32_t throwMs)
{
	constexpr float k_SpeedFactor = 1.6f;
	constexpr int32_t k_EaseMs = 400;
	constexpr float k_EasePerMs = 0.0025f;
	const float handSpeed = ((highWeight * hands.highSpeed) + ((1.0f - highWeight) * hands.flatSpeed)) * scale;
	const auto speedStep = x87::ToInteger(((static_cast<float>(turnMs) * lift.speed) * k_SpeedFactor) / handSpeed);
	const auto apart = std::min(std::abs(timeMs - throwMs), k_EaseMs);
	const float ease = static_cast<float>(apart) * k_EasePerMs;
	return x87::ToInteger(x87::Add(x87::Mul(static_cast<double>(clipStepMs - speedStep), ease), speedStep));
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
	const auto hasChild = [&parents](uint32_t bone) {
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

glm::vec3 creature_throw::TossVelocity(const glm::vec3& handVelocity, bool mirrored, GameHeading heading, float scale,
                                       float share)
{
	const float side = mirrored ? -(handVelocity.x * share) : handVelocity.x * share;
	const float across = side * scale;
	const float up = (handVelocity.y * share) * scale;
	const float ahead = (handVelocity.z * share) * scale;
	const float cosine = x87::RoundToFloat(x87::Cos(heading.Radians()));
	const double sine = x87::Sin(heading.Radians());
	return {x87::Mul(-sine, ahead) + (cosine * across), up, (cosine * ahead) + x87::Mul(sine, across)};
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
		// Right on the thing's centre the way has no length, so the move fails (deviation 79)
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

std::optional<glm::vec2> creature_throw::BackOffPoint(const creature_route::WalkableLand& land, glm::vec2 point)
{
	if (land.IsValid(point, creature_route::k_DestinationClearance))
	{
		return point;
	}
	const auto valid = land.NearestStandable(point);
	if (!valid.has_value())
	{
		return std::nullopt;
	}
	// How far away the place is, with the square of z first
	const float acrossX = point.x - valid->x;
	const float acrossZ = point.y - valid->y;
	const float away = std::sqrt((acrossZ * acrossZ) + (acrossX * acrossX));
	if (!(away < creature_route::k_ValidPointReach))
	{
		return std::nullopt;
	}
	return valid;
}
