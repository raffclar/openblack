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

#include <ranges>

using namespace openblack;
using namespace openblack::camera_shake;

int32_t camera_shake::ScriptMilliseconds(float seconds)
{
	const float milliseconds = seconds * 1000.0f;
	return static_cast<int32_t>(std::lrint(milliseconds));
}

int32_t camera_shake::EffectMilliseconds(float seconds)
{
	const float milliseconds = seconds * 1000.0f;
	return static_cast<int32_t>(milliseconds);
}

void camera_shake::Advance(std::vector<Shake>& shakes, int32_t frameMilliseconds)
{
	std::erase_if(shakes, [frameMilliseconds](Shake& shake) {
		shake.millisecondsLeft -= frameMilliseconds;
		return shake.millisecondsLeft <= 0;
	});
}

namespace
{

/// The distance from the camera to the shake, with the squares added in the order z, y, x
float Distance(const glm::vec3& shake, const glm::vec3& camera)
{
	const glm::vec3 offset = shake - camera;
	const float squares = offset.z * offset.z + offset.y * offset.y;
	return std::sqrt(squares + offset.x * offset.x);
}

} // namespace

const Shake* camera_shake::Nearest(std::span<const Shake> shakes, const glm::vec3& camera)
{
	// The newest shake is first, and only a shake that is strictly nearer replaces the nearest
	const Shake* nearest = nullptr;
	float best = 0.0f;
	for (const auto& shake : std::ranges::reverse_view(shakes))
	{
		const float distance = Distance(shake.position, camera);
		if (nearest == nullptr || distance < best)
		{
			nearest = &shake;
			best = distance;
		}
	}
	return nearest;
}

float camera_shake::Amplitude(std::span<const Shake> shakes, const glm::vec3& camera)
{
	const Shake* nearest = Nearest(shakes, camera);
	// Only a camera that is strictly in the radius of the shake shakes
	if (nearest == nullptr || !(Distance(nearest->position, camera) < nearest->radius) || nearest->milliseconds <= 0)
	{
		return 0.0f;
	}
	// The share of the time that is left, then times the strength
	const float share = static_cast<float>(nearest->millisecondsLeft) / static_cast<float>(nearest->milliseconds);
	return share * nearest->strength;
}

Offsets camera_shake::Jitter(float amplitude, bool verticalOnly, const std::function<float(float, float)>& random)
{
	if (amplitude <= 0.0f)
	{
		return {};
	}
	if (verticalOnly)
	{
		const float eye = random(-amplitude, amplitude);
		const float focus = random(-amplitude, amplitude);
		return {.eye = {0.0f, eye, 0.0f}, .focus = {0.0f, focus, 0.0f}};
	}
	Offsets offsets;
	offsets.eye.z = random(-amplitude, amplitude);
	offsets.eye.y = random(-amplitude, amplitude);
	offsets.eye.x = random(-amplitude, amplitude);
	offsets.focus.z = random(-amplitude, amplitude);
	offsets.focus.y = random(-amplitude, amplitude);
	offsets.focus.x = random(-amplitude, amplitude);
	return offsets;
}
