/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraZones.h"

#include <cmath>

#include <utility>

using namespace openblack;
using namespace openblack::camera_zones;

namespace
{
/// A point this close to a corner, squared, is on the fence; a side shorter than this, squared, is skipped
constexpr float k_OnCorner = 1e-8f;
/// A line this near parallel to a side doesn't cross it, and a crossing this near the point is the point
constexpr float k_Parallel = 1e-4f;
/// The camera has hardly moved this frame when it is this near, squared, across the ground to where it started
constexpr float k_HardlyMoved = 1e-4f;
/// The line between the camera and what it looks at must rise or fall more than this for the camera to slide down it
constexpr float k_SteepEnough = 2.0f;
/// The search round the crossing turns by a 64th of a turn, up to 31 of them either way
constexpr float k_SearchStep = 0.0981747732f;
constexpr int k_SearchSteps = 32;

float DistanceSquaredAcross(const glm::vec3& a, const glm::vec3& b)
{
	const float x = a.x - b.x;
	const float z = a.z - b.z;
	return x * x + z * z;
}
} // namespace

Crossing camera_zones::CrossFence(std::span<const glm::vec3> fence, bool fenceOn, const glm::vec3& point,
                                  const glm::vec3& direction)
{
	Crossing crossing {.inside = true, .closest = point};
	if (!fenceOn || fence.size() <= 2)
	{
		return crossing;
	}
	uint32_t ahead = 0;
	float nearestAhead = 1e20f;
	float nearestBehind = 1e20f;
	std::optional<std::pair<glm::vec3, glm::vec3>> hitAhead;
	std::optional<std::pair<glm::vec3, glm::vec3>> hitBehind;
	const auto* previous = &fence.back();
	for (const auto& corner : fence)
	{
		const float sideX = previous->x - corner.x;
		const float sideZ = previous->z - corner.z;
		const float sideSquared = sideZ * sideZ + sideX * sideX;
		if (DistanceSquaredAcross(point, corner) < k_OnCorner)
		{
			return crossing;
		}
		if (k_OnCorner < sideSquared)
		{
			const float across = -sideX;
			const float facing = direction.x * sideZ + direction.z * across;
			if (k_Parallel < std::abs(facing))
			{
				// How far along the line it meets the side, and how far along the side from this corner
				const float along = (((corner.z * across + corner.x * sideZ) - point.x * sideZ) - point.z * across) / facing;
				const glm::vec3 hit {along * direction.x + point.x, direction.y * along + point.y,
				                     along * direction.z + point.z};
				const float onSide = ((hit.z - corner.z) * sideZ + (hit.x - corner.x) * sideX) / sideSquared;
				if (0.0f <= onSide && onSide < 1.0f)
				{
					if (std::abs(along) < k_Parallel)
					{
						return crossing;
					}
					const glm::vec3 normal {sideZ, 0.0f, across};
					if (along > 0.0f)
					{
						++ahead;
						if (along < nearestAhead)
						{
							nearestAhead = along;
							hitAhead = {hit, normal};
						}
					}
					else if (-along < nearestBehind)
					{
						nearestBehind = -along;
						hitBehind = {hit, normal};
					}
				}
			}
		}
		previous = &corner;
	}
	bool found = false;
	if (const auto& hit = hitAhead.has_value() ? hitAhead : hitBehind; hit.has_value())
	{
		crossing.closest = hit->first;
		crossing.normal = hit->second;
		found = true;
	}
	// A corner nearer than the crossing is nearer still
	float nearest = DistanceSquaredAcross(crossing.closest, point);
	for (const auto& corner : fence)
	{
		const float distance = DistanceSquaredAcross(corner, point);
		if (distance < nearest || !found)
		{
			crossing.closest = corner;
			found = true;
			nearest = distance;
		}
	}
	crossing.inside = (ahead & 1u) != 0u;
	return crossing;
}

float camera_zones::HeightLimit(const Zones& zones, float ground)
{
	const float highest = zones.useMaxAltitude ? zones.maxAltitude : k_NoLimit;
	const float aboveLand = zones.useHeightAboveLand ? zones.heightAboveLand : k_NoLimit;
	return ground + aboveLand <= highest ? ground + aboveLand : highest;
}

std::optional<glm::vec3> camera_zones::SlideUnderLimit(const glm::vec3& origin, const glm::vec3& focus, float limit)
{
	if (!(origin.y > limit))
	{
		return std::nullopt;
	}
	const auto line = focus - origin;
	if (!(std::abs(line.y) > k_SteepEnough))
	{
		return std::nullopt;
	}
	return line * -((origin.y - limit) / line.y);
}

std::optional<glm::vec3> camera_zones::PushInsideFence(std::span<const glm::vec3> fence, bool fenceOn,
                                                       const glm::vec3& originAtStart, const glm::vec3& origin,
                                                       const glm::vec3& focus, float searchRadius)
{
	const auto towards = DistanceSquaredAcross(originAtStart, origin) < k_HardlyMoved ? focus : originAtStart;
	const auto direction = towards - origin;
	const auto crossing = CrossFence(fence, fenceOn, origin, direction);
	if (crossing.inside)
	{
		return std::nullopt;
	}
	// The way across the ground from the camera to the crossing
	glm::vec3 way {crossing.closest.x - origin.x, 0.0f, crossing.closest.z - origin.z};
	if (way.x != 0.0f || way.z != 0.0f)
	{
		way *= 1.0f / std::sqrt(way.x * way.x + way.z * way.z);
	}
	auto target = crossing.closest;
	bool searching = true;
	for (int step = 0; searching && step < k_SearchSteps; ++step)
	{
		for (const int turn : {-step, step})
		{
			const double angle = static_cast<double>(turn) * static_cast<double>(k_SearchStep);
			const auto sine = static_cast<float>(std::sin(angle) * static_cast<double>(searchRadius));
			const auto cosine = static_cast<float>(std::cos(angle) * static_cast<double>(searchRadius));
			const glm::vec3 candidate =
			    crossing.closest + glm::vec3(cosine * way.x - sine * way.z, 0.0f, sine * way.x + cosine * way.z);
			if (CrossFence(fence, fenceOn, candidate, direction).inside)
			{
				target = candidate;
				searching = false;
				break;
			}
		}
	}
	return glm::vec3(target.x - origin.x, 0.0f, target.z - origin.z);
}

glm::vec3 camera_zones::FlightOriginInsideFence(std::span<const glm::vec3> fence, bool fenceOn, const glm::vec3& origin,
                                                const glm::vec3& focus)
{
	auto direction = focus - origin;
	if (direction != glm::vec3(0.0f))
	{
		direction *= 1.0f / std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
	}
	const auto crossing = CrossFence(fence, fenceOn, origin, direction);
	return crossing.inside ? origin : crossing.closest;
}

bool camera_zones::ScriptInfluenceCounts(std::span<const glm::vec3> fence, bool fenceOn, const glm::vec3& point)
{
	return CrossFence(fence, fenceOn, point, glm::vec3(1.0f, 0.0f, 0.0f)).inside;
}
