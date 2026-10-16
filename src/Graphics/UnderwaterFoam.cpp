/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "UnderwaterFoam.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

namespace openblack::graphics::underwater
{

glm::vec2 FoamFocus(glm::vec3 eye, glm::vec3 forward, float seaLevel)
{
	const glm::vec2 under {eye.x, eye.z};
	// A view that looks up, or that is near the level, does not meet the water near enough
	constexpr float k_LeastDown = 0.01f;
	if (forward.y > -k_LeastDown || eye.y <= seaLevel)
	{
		return under;
	}
	const float along = (seaLevel - eye.y) / forward.y;
	const glm::vec2 out = glm::vec2 {forward.x, forward.z} * along;
	const float length = glm::length(out);
	if (length <= k_FoamFarthestFocus)
	{
		return under + out;
	}
	return under + out * (k_FoamFarthestFocus / length);
}

glm::vec2 FoamWindowOrigin(glm::vec2 focus)
{
	const glm::vec2 corner = focus - glm::vec2(k_FoamWindow * 0.5f);
	return glm::floor(corner / k_FoamTexelSize) * k_FoamTexelSize;
}

std::optional<FoamStep> FoamClock::Advance(float seconds, glm::vec2 focus, uint32_t revision)
{
	const auto origin = FoamWindowOrigin(focus);
	// A new land, or a camera that jumps more than half the field, starts the field again
	const bool jumped = glm::length(origin - _origin) > k_FoamWindow * 0.5f;
	const bool fresh = !_revision.has_value() || *_revision != revision || !_last.has_value();
	if (fresh || jumped || seconds < *_last)
	{
		_last = seconds;
		_waiting = 0.0f;
		_revision = revision;
		_target = static_cast<uint8_t>(1 - _target);
		const FoamStep step {.seconds = 0.0f, .origin = origin, .previousOrigin = origin, .clear = true, .target = _target};
		_origin = origin;
		return step;
	}
	_waiting += seconds - *_last;
	_last = seconds;
	if (_waiting < k_FoamStepSeconds)
	{
		return std::nullopt;
	}
	const float covered = std::min(_waiting, k_FoamLongestStep);
	_waiting = 0.0f;
	_target = static_cast<uint8_t>(1 - _target);
	const FoamStep step {.seconds = covered, .origin = origin, .previousOrigin = _origin, .clear = false, .target = _target};
	_origin = origin;
	return step;
}

float SedimentStir(float swash, float storm)
{
	const float waves = std::max(swash, 0.0f) / k_CalmSwash;
	return std::clamp(waves * (0.6f + 0.8f * std::clamp(storm, 0.0f, 1.0f)), 0.0f, k_MostStir);
}

} // namespace openblack::graphics::underwater
