/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraShake.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

#include "Common/GameRandom.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "Locator.h"

namespace openblack::camera_shake
{
namespace
{
/// The camera shakes, newest first (Locator::scriptState)
struct CameraShakeState
{
	std::vector<Checker> list;
};

std::vector<Checker>& List()
{
	return Locator::scriptState::value().Get<CameraShakeState>().list;
}

/// The length of a - b, summed as (z z + y y) + x x
float Distance(const glm::vec3& a, const glm::vec3& b)
{
	const auto d = a - b;
	return std::sqrt(d.z * d.z + d.y * d.y + d.x * d.x);
}
} // namespace

void Create(float maxDistance, const glm::vec3& point, float amplitude, int32_t ms, bool yOnly)
{
	Checker checker {
	    .maxDistance = maxDistance,
	    .point = point,
	    .amplitude = amplitude,
	    .totalMs = ms,
	    .remainingMs = ms,
	    .yOnly = yOnly,
	};
	auto& list = List();
	list.insert(list.begin(), checker); // the head of the list
}

void StartCameraShake(const glm::vec3& point, float radius, float amplitude, float seconds)
{
	// the ms are stored as a float, then rounded to nearest
	const float ms = seconds * k_MsPerSecond;
	Create(radius, point, amplitude, static_cast<int32_t>(std::lrint(ms)), false);
}

void Adjust(const glm::vec3& lastDrawn, glm::vec3& position, glm::vec3& target)
{
	const auto& list = List();
	if (list.empty()) // the shakes on/off switch is always on
	{
		return;
	}
	// the nearest to the last drawn camera; a later one only when strictly nearer
	const Checker* nearest = &list.front();
	float best = Distance(nearest->point, lastDrawn);
	for (auto it = std::next(list.begin()); it != list.end(); ++it)
	{
		if (const float d = Distance(it->point, lastDrawn); d < best)
		{
			best = d;
			nearest = &*it;
		}
	}
	if (!(best < nearest->maxDistance))
	{
		return;
	}
	if (nearest->totalMs == 0)
	{
		// Not original: a shake of 0 ms divides 0 by 0 and draws a NaN camera until Tick frees it; the
		// original's frame is not reproduced here
		return;
	}
	// remaining / total x amplitude
	const float a = static_cast<float>(nearest->remainingMs) / static_cast<float>(nearest->totalMs) * nearest->amplitude;
	if (nearest->yOnly)
	{
		position.y += game_random::crt::Random(-a, a);
		target.y += game_random::crt::Random(-a, a);
		return;
	}
	// Six draws, kept in this order
	const float positionZ = game_random::crt::Random(-a, a);
	const float positionY = game_random::crt::Random(-a, a);
	const float positionX = game_random::crt::Random(-a, a);
	position.x += positionX;
	position.y += positionY;
	position.z += positionZ;
	const float targetZ = game_random::crt::Random(-a, a);
	const float targetY = game_random::crt::Random(-a, a);
	const float targetX = game_random::crt::Random(-a, a);
	target.x += targetX;
	target.y += targetY;
	target.z += targetZ;
}

void Tick(uint32_t frameMs)
{
	auto& list = List();
	// a shake is kept while its remaining ms stay above 0
	for (auto& checker : list)
	{
		checker.remainingMs -= static_cast<int32_t>(frameMs);
	}
	list.erase(std::remove_if(list.begin(), list.end(), [](const Checker& c) { return c.remainingMs <= 0; }), list.end());
}

void Reset()
{
	List().clear();
}

const std::vector<Checker>& Checkers()
{
	return List();
}

} // namespace openblack::camera_shake
