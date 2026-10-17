/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the shadows of the sun and the moon

#include "SunShadows.h"

#include <cmath>

#include <algorithm>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>

using namespace openblack::graphics;
using namespace openblack::graphics::sun_shadows;

ShadowClock::Frame ShadowClock::Advance(float seconds, glm::vec3 towardsLight, glm::vec2 focus)
{
	const auto light = glm::normalize(towardsLight);
	bool update = !_last.has_value();
	if (!update)
	{
		const float turned = std::acos(std::clamp(glm::dot(light, _light), -1.0f, 1.0f));
		const bool late = seconds - *_last >= k_UpdateSeconds;
		const bool moved = glm::length(focus - _focus) > k_CascadeWidths.front() * 0.25f;
		// The new maps must have blended in before the next ones start
		const bool blended = seconds - *_last >= k_BlendSeconds;
		update = blended && (late || turned > k_TurnRadians || moved);
	}
	const bool first = !_last.has_value();
	if (update)
	{
		_newest = static_cast<uint8_t>(1 - _newest);
		_last = seconds;
		_focus = focus;
		_light = light;
	}
	Frame frame;
	frame.update = update;
	frame.newest = _newest;
	frame.blend = first ? 1.0f : std::clamp((seconds - *_last) / k_BlendSeconds, 0.0f, 1.0f);
	return frame;
}

glm::vec3 sun_shadows::SnapToTexels(glm::vec3 middle, glm::vec3 right, glm::vec3 up, float width)
{
	const float texel = width / static_cast<float>(k_MapTexels);
	const float across = glm::dot(middle, right);
	const float along = glm::dot(middle, up);
	const float snappedAcross = std::round(across / texel) * texel;
	const float snappedAlong = std::round(along / texel) * texel;
	return middle + right * (snappedAcross - across) + up * (snappedAlong - along);
}

LightView sun_shadows::CascadeView(glm::vec3 towardsLight, glm::vec3 middle, float width, bool homogeneousDepth)
{
	const auto light = glm::normalize(towardsLight);
	const glm::vec3 up = std::abs(light.y) > 0.99f ? glm::vec3 {0.0f, 0.0f, 1.0f} : glm::vec3 {0.0f, 1.0f, 0.0f};
	auto view = glm::lookAt(middle + light * k_LightDistance, middle, up);
	// The middle on whole texels of the light's view
	const glm::vec3 right {view[0][0], view[1][0], view[2][0]};
	const glm::vec3 viewUp {view[0][1], view[1][1], view[2][1]};
	const auto snapped = SnapToTexels(middle, right, viewUp, width);
	view = glm::lookAt(snapped + light * k_LightDistance, snapped, up);
	const float half = width * 0.5f;
	glm::mat4 projection {1.0f};
	glm::mat4 reverse {1.0f};
	if (homogeneousDepth)
	{
		projection = glm::orthoNO(-half, half, -half, half, 1.0f, k_LightDepth);
		// The depth from -1 at the near plane to 1 at the far becomes 1 at the near plane and -1 at the far
		reverse[2][2] = -1.0f;
	}
	else
	{
		projection = glm::orthoZO(-half, half, -half, half, 1.0f, k_LightDepth);
		// The depth from 0 at the near plane to 1 at the far becomes 1 at the near plane and 0 at the far
		reverse[2][2] = -1.0f;
		reverse[3][2] = 1.0f;
	}
	return {.view = view, .projection = reverse * projection};
}

float sun_shadows::VisibilityAt(glm::vec2 point, glm::vec3 towardsLight, const std::function<float(glm::vec2)>& heightAt)
{
	if (towardsLight.y <= 0.0f)
	{
		return 0.0f;
	}
	const glm::vec2 across {towardsLight.x, towardsLight.z};
	const float flat = glm::length(across);
	if (flat < 1e-4f)
	{
		return 1.0f;
	}
	const auto direction = across / flat;
	const float rise = towardsLight.y / flat;
	const float ground = heightAt(point);
	float visibility = 1.0f;
	// The steps grow, so that the far hills count with few samples
	float t = 8.0f;
	for (int i = 0; i < 40 && t < 3500.0f; ++i)
	{
		const float above = heightAt(point + direction * t) - (ground + t * rise);
		const float penumbra = 0.5f + t * 0.02f;
		visibility = std::min(visibility, std::clamp(0.5f - above / penumbra, 0.0f, 1.0f));
		t *= 1.2f;
	}
	return visibility;
}
