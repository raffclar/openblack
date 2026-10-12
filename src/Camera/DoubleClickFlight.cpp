/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DoubleClickFlight.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <numbers>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

using namespace openblack;
using namespace openblack::camera_fly;

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;
/// Nearly straight above, a camera looks with no heading and this pitch
constexpr float k_StraightAbove = 0.01f;
constexpr float k_NearlyStraightDown = 1.53938043f;
/// The headings scored, and the points out along each of them
constexpr int k_Headings = 32;
constexpr int k_FirstPoint = 3;
constexpr int k_LastPoint = 7;
constexpr float k_PointStep = 0.125f;
/// The turn from the camera's heading is weighed by this times its cosine
constexpr float k_TurnWeight = 50.0f;
/// The pitch a flight ends with
constexpr float k_OldPitchShare = 0.2f;
constexpr float k_SlopeShare = 0.2f;
constexpr double k_PitchBase = 0.37699114390179034;
constexpr float k_BestMinPitch = k_Pi / 8.0f;
constexpr float k_BestMaxPitch = k_Pi / 3.0f;
} // namespace

Bearing camera_fly::BearingOf(const glm::vec3& origin, const glm::vec3& focus)
{
	const auto diff = origin - focus;
	if (std::abs(diff.x) < k_StraightAbove && std::abs(diff.z) < k_StraightAbove)
	{
		return {.heading = 0.0f, .pitch = k_NearlyStraightDown};
	}
	return {.heading = k_Pi - std::atan2(diff.x, -diff.z), .pitch = std::atan2(diff.y, std::hypot(diff.x, diff.z))};
}

glm::vec3 camera_fly::OriginAt(const glm::vec3& focus, float distance, const Bearing& bearing)
{
	const float level = std::cos(bearing.pitch);
	return {std::sin(bearing.heading) * level * distance + focus.x, std::sin(bearing.pitch) * distance + focus.y,
	        std::cos(bearing.heading) * level * distance + focus.z};
}

Bearing camera_fly::BestBearing(const camera_limits::Ground& ground, const Bearing& current, float distance,
                                const glm::vec3& place)
{
	std::array<float, k_Headings> scores {};
	for (int i = 0; i < k_Headings; ++i)
	{
		const float heading = static_cast<float>(i) * k_Pi / 16.0f + current.heading;
		for (int j = k_FirstPoint; j <= k_LastPoint; ++j)
		{
			const auto point =
			    OriginAt(place, static_cast<float>(j) * distance * k_PointStep, {.heading = heading, .pitch = 0.0f});
			scores.at(i) += place.y - ground.height({point.x, point.z});
		}
	}
	int best = 0;
	float bestScore = -1.0e20f;
	for (int i = 0; i < k_Headings; ++i)
	{
		scores.at(i) += static_cast<float>(std::cos(static_cast<double>(static_cast<float>(i) * k_Pi / 16.0f)) * k_TurnWeight);
		if (scores.at(i) > bestScore)
		{
			bestScore = scores.at(i);
			best = i;
		}
	}

	// The slope of the land at the place: the pitch of its normal, as if the camera stood one along it
	const auto normal = ground.normal({place.x, place.z});
	const float slopePitch = BearingOf(normal + place, place).pitch * 0.5f;
	const float oldPitch = current.pitch * k_OldPitchShare;
	const auto pitch = static_cast<float>(static_cast<double>(oldPitch) +
	                                      static_cast<double>(slopePitch) * static_cast<double>(k_SlopeShare) + k_PitchBase);
	return {.heading = static_cast<float>(best) * k_Pi / 16.0f + current.heading,
	        .pitch = std::clamp(pitch, k_BestMinPitch, k_BestMaxPitch)};
}

Flight camera_fly::Plan(Reason reason, const camera_limits::Ground& ground, const glm::vec3& place, const glm::vec3& origin,
                        const glm::vec3& focus, const glm::vec3& screenCentre)
{
	const float focusDistance = glm::distance(place, screenCentre);
	const float originDistance = glm::distance(place, origin);
	Flight flight {.focus = place};
	float distance = originDistance;
	bool bestBearing = true;
	if (originDistance > k_FarDistance * k_DistanceFactor && focusDistance > k_FarFocusMinimum)
	{
		flight.startSeconds = k_LongStart;
		distance = k_FarDistance;
		flight.woosh = true;
	}
	else if (originDistance > k_MediumDistance * k_DistanceFactor)
	{
		flight.startSeconds = k_ShortStart;
		distance = k_MediumDistance;
	}
	else
	{
		flight.startSeconds = k_ShortStart;
		distance = originDistance > k_NearDistance * k_DistanceFactor ? k_NearDistance : k_CloseDistance;
	}
	if (reason == Reason::GripTooFar)
	{
		flight.startSeconds = k_GripStart;
		distance = k_GripDistance;
		bestBearing = false;
		flight.woosh = true;
	}

	auto bearing = BearingOf(origin, focus);
	if (bestBearing)
	{
		bearing = BestBearing(ground, bearing, distance, place);
	}
	bearing.pitch = std::clamp(bearing.pitch, k_MinPitch, k_MaxPitch);
	flight.origin = OriginAt(place, distance, bearing);
	flight.halfWay = flight.startSeconds == k_LongStart;
	return flight;
}

std::optional<glm::vec3> camera_fly::Destination(std::optional<glm::vec3> place, const camera_zones::Zones& zones)
{
	if (!place.has_value())
	{
		return std::nullopt;
	}
	auto destination = *place;
	if (const auto crossing = camera_zones::CrossFence(zones.fence, zones.fenceOn, destination, {1.0f, 0.0f, 0.0f});
	    !crossing.inside)
	{
		destination = crossing.closest;
	}
	const float x = destination.x - camera_limits::k_AllowedAreaCentre.x;
	const float z = destination.z - camera_limits::k_AllowedAreaCentre.y;
	if (!(x * x + z * z < camera_limits::k_AllowedAreaRadius * camera_limits::k_AllowedAreaRadius))
	{
		return std::nullopt;
	}
	return destination;
}

glm::vec3 camera_fly::HalfWayPoint(const glm::vec3& end, const glm::vec3& current, float rise,
                                   const std::function<float(glm::vec2)>& groundHeight)
{
	auto point = (end + current) * 0.5f;
	point.y += std::hypot(current.x - end.x, current.z - end.z) * rise;
	const float clear = groundHeight({point.x, point.z}) + k_HalfWayClearance;
	if (clear > point.y)
	{
		point.y = clear;
	}
	return point;
}
