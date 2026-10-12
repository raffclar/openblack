/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraLimits.h"

#include <cmath>

#include <algorithm>
#include <array>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

using namespace openblack;
using namespace openblack::camera_limits;
using camera_zones::Exclusion;

namespace
{
/// The feeling points: rings from straight below the camera to straight above, an eighth of a turn apart, with this
/// many points round each
constexpr std::array<int, 5> k_RingPoints {1, 4, 6, 4, 1};
/// A cylinder's push measures the camera's distance from its axis as at least this
constexpr float k_NearestToAxis = 0.01f;
/// A point is felt in the land only when deeper than this, so that a camera resting at its clearance over the land isn't
/// pushed by the rounding of the maths that turned it there
constexpr float k_Rounding = 1e-4f;

float SquaredAcross(const glm::vec3& a, const glm::vec3& b)
{
	const float x = a.x - b.x;
	const float z = a.z - b.z;
	return x * x + z * z;
}

/// The dome's roof over a point across the ground, where the point is under it
std::optional<float> DomeRoof(const Exclusion& dome, glm::vec2 point)
{
	const float x = point.x - dome.position.x;
	const float z = point.y - dome.position.z;
	const float radiusSquared = dome.radius * dome.radius;
	const float across = x * x + z * z;
	if (!(across < radiusSquared))
	{
		return std::nullopt;
	}
	return std::sqrt(1.0f - across / radiusSquared) * dome.height + dome.position.y;
}
} // namespace

std::optional<glm::vec3> camera_limits::KeepInsideAllowedArea(const glm::vec3& origin)
{
	const float x = origin.x - k_AllowedAreaCentre.x;
	const float z = origin.z - k_AllowedAreaCentre.y;
	const float squared = z * z + x * x;
	if (!(squared > k_AllowedAreaRadius * k_AllowedAreaRadius))
	{
		return std::nullopt;
	}
	const float scale = k_AllowedAreaRadius / std::sqrt(squared);
	return glm::vec3(x * scale + k_AllowedAreaCentre.x, origin.y, z * scale + k_AllowedAreaCentre.y);
}

std::optional<glm::vec3> camera_limits::KeepInsideTheWorld(const glm::vec3& destination)
{
	const auto fromCentre = destination - k_WorldCentre;
	const float squared = glm::dot(fromCentre, fromCentre);
	if (!(squared > k_WorldReach * k_WorldReach))
	{
		return std::nullopt;
	}
	return k_WorldCentre + fromCentre * (k_WorldHoldRadius / std::sqrt(squared));
}

float camera_limits::EaseSeconds(float sinceFlight, bool pushed, bool focusMoved)
{
	float seconds = k_EaseSeconds;
	if (focusMoved)
	{
		seconds *= 2.0f;
	}
	if (pushed)
	{
		seconds *= 2.0f;
	}
	if (sinceFlight <= k_EaseSettleSeconds)
	{
		seconds = (seconds - k_FlightEaseSeconds) * (sinceFlight / k_EaseSettleSeconds) + k_FlightEaseSeconds;
	}
	return seconds;
}

float camera_limits::FrameMoveLimit(float focusDistance, float seconds)
{
	return std::max(k_MinFrameMove, k_FrameMovePerFocusDistance * focusDistance * seconds);
}

std::optional<glm::vec3> camera_limits::LimitFrameMove(const glm::vec3& start, const glm::vec3& origin, float limit)
{
	const auto move = origin - start;
	if (!(glm::dot(move, move) > limit * limit))
	{
		return std::nullopt;
	}
	return start + glm::normalize(move) * limit;
}

std::vector<Exclusion> camera_limits::NearbyExclusions(std::span<const Exclusion> exclusions, const glm::vec3& originAtStart)
{
	constexpr float k_Reach = k_Clearance * 2.0f;
	std::vector<Exclusion> nearby;
	for (const auto& exclusion : exclusions)
	{
		const float reach = k_Reach + exclusion.radius;
		if (!(SquaredAcross(exclusion.position, originAtStart) < reach * reach))
		{
			continue;
		}
		if (exclusion.kind != Exclusion::Kind::Dome || originAtStart.y - exclusion.position.y < k_Reach + exclusion.height)
		{
			nearby.push_back(exclusion);
		}
	}
	return nearby;
}

std::optional<glm::vec3> camera_limits::PushOutOfCylinders(std::span<const Exclusion> nearby, const glm::vec3& origin)
{
	auto pushed = origin;
	bool any = false;
	for (const auto& cylinder : nearby)
	{
		const float reach = k_Clearance + cylinder.radius;
		if (cylinder.kind != Exclusion::Kind::Cylinder || !(SquaredAcross(cylinder.position, pushed) < reach * reach))
		{
			continue;
		}
		const float x = pushed.x - cylinder.position.x;
		const float z = pushed.z - cylinder.position.z;
		const float scale = reach / std::max(std::sqrt(z * z + x * x), k_NearestToAxis);
		pushed.x = cylinder.position.x + x * scale;
		pushed.z = cylinder.position.z + z * scale;
		any = true;
	}
	return any ? std::optional(pushed) : std::nullopt;
}

float camera_limits::SurfaceHeight(const Ground& ground, std::span<const Exclusion> nearby, bool withDomes, glm::vec2 point)
{
	float height = ground.height(point);
	if (!withDomes)
	{
		return height;
	}
	for (const auto& dome : nearby)
	{
		if (dome.kind != Exclusion::Kind::Dome)
		{
			continue;
		}
		if (const auto roof = DomeRoof(dome, point); roof.has_value() && height <= *roof)
		{
			height = *roof;
		}
	}
	return height;
}

GroundPush camera_limits::PushOffTheGround(const Ground& ground, std::span<const Exclusion> nearby, bool exclusionsOn,
                                           const glm::vec3& origin, const glm::vec3& focus)
{
	GroundPush result {.origin = origin, .focus = focus};
	// The ball faces the way the camera looks: `back` from what it looks at to the camera
	const auto back = glm::normalize(origin - focus);
	const auto side = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), back));
	const auto up = glm::normalize(glm::cross(back, side));
	constexpr float k_MaxPush = k_Clearance * 0.5f;
	for (int ring = -2; ring <= 2; ++ring)
	{
		const float ringAngle = static_cast<float>(ring) * glm::quarter_pi<float>();
		const float ringCos = std::cos(ringAngle);
		const float ringSin = std::sin(ringAngle);
		const int count = k_RingPoints.at(static_cast<size_t>(ring + 2));
		for (int i = 0; i < count; ++i)
		{
			const float angle = static_cast<float>(i) * glm::two_pi<float>() / static_cast<float>(count);
			const auto point = result.origin + k_Clearance * (side * (std::sin(angle) * ringCos) + up * ringSin +
			                                                  back * (std::cos(angle) * ringCos));
			const glm::vec2 across {point.x, point.z};
			if (!(SurfaceHeight(ground, nearby, exclusionsOn, across) > point.y + k_Rounding))
			{
				continue;
			}
			// The surface's height and normal here: a dome's roof gives its own, facing out from its middle, wherever it
			// stands above the land
			float surface = ground.height(across);
			auto normal = ground.normal(across);
			for (const auto& dome : nearby)
			{
				if (dome.kind != Exclusion::Kind::Dome)
				{
					continue;
				}
				if (const auto roof = DomeRoof(dome, across); roof.has_value() && *roof > surface)
				{
					normal = glm::normalize(point - dome.position);
					surface = *roof;
				}
			}
			const float depth = std::min(glm::dot(normal, glm::vec3(point.x, surface, point.z) - point), k_MaxPush);
			const auto push = normal * depth;
			result.origin += push;
			result.pushed = true;
			if (const float backwards = glm::dot(push, back); backwards > 0.0f)
			{
				result.focus += push * backwards;
				result.focusMoved = true;
			}
		}
	}
	return result;
}
