/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LearningMarker.h"

#include <cmath>

#include <algorithm>
#include <vector>

#include "Common/FixedFormat.h"
#include "ECS/Components/LearningMarker.h"
#include "ECS/Registry.h"

namespace openblack::ecs::learning_marker
{

namespace
{
/// The mark's age is turned into the bulb's blink by this many radians a millisecond: one turn a second
constexpr double k_BlinkPerMillisecond = 0.0062831854;
} // namespace

std::string Text(float share)
{
	// In hundredths, with no point, rounded as the game's C library does
	return fixed_format::Fixed(static_cast<double>(share) * 100.0, 0, 0) + "%";
}

components::LearningMarker Start(const glm::vec3& head, float creatureHeight, float share)
{
	return {.text = Text(share),
	        .position = head + glm::vec3(0.0f, k_OverHead, 0.0f),
	        .size = creatureHeight * k_SizeOfHeight,
	        .milliseconds = 0.0f};
}

bool Step(components::LearningMarker& marker, float milliseconds)
{
	marker.milliseconds += milliseconds;
	if (marker.milliseconds >= k_LifeMilliseconds)
	{
		return false;
	}
	marker.position.y += milliseconds * k_RisePerMillisecond * marker.size;
	return true;
}

float Grown(const components::LearningMarker& marker)
{
	return std::max((marker.milliseconds / k_LifeMilliseconds + 1.0f) * marker.size, k_MinimumSize);
}

uint8_t TextAlpha(float milliseconds)
{
	if (milliseconds < k_FadeStartMilliseconds)
	{
		return 0xFF;
	}
	const double left =
	    1.0 - (static_cast<double>(milliseconds) - k_FadeStartMilliseconds) / (k_LifeMilliseconds - k_FadeStartMilliseconds);
	const auto alpha = static_cast<int32_t>(left * 255.0);
	return static_cast<uint8_t>(std::clamp(alpha, 0, 0xFF));
}

uint8_t BulbWhite(float milliseconds)
{
	return static_cast<uint8_t>(std::abs(std::cos(static_cast<double>(milliseconds) * k_BlinkPerMillisecond)) * 255.0);
}

uint8_t GlowAlpha(float milliseconds)
{
	return static_cast<uint8_t>((0xFF - BulbWhite(milliseconds)) / 2);
}

Look LookOf(const components::LearningMarker& marker)
{
	const float grown = Grown(marker);
	const float white = static_cast<float>(BulbWhite(marker.milliseconds)) / 255.0f;
	const float glow = static_cast<float>(GlowAlpha(marker.milliseconds)) / 255.0f;
	return {
	    .glow = marker.position,
	    .glowHalfSize = grown,
	    .glowColour = glm::vec4(1.0f, 1.0f, 0.0f, glow),
	    .bulb = marker.position - glm::vec3(0.0f, grown * k_BulbUnder, 0.0f),
	    .bulbScale = grown * k_BulbScale,
	    .bulbColour = glm::vec4(1.0f, 1.0f, white, 1.0f),
	    .textPoint = marker.position + glm::vec3(0.0f, grown * k_TextOver, 0.0f),
	    .textAlpha = static_cast<float>(TextAlpha(marker.milliseconds)) / 255.0f,
	};
}

void StepAll(Registry& registry, float milliseconds)
{
	std::vector<entt::entity> gone;
	registry.Each<components::LearningMarker>([milliseconds, &gone](entt::entity entity, components::LearningMarker& marker) {
		if (!Step(marker, milliseconds))
		{
			gone.push_back(entity);
		}
	});
	for (const auto entity : gone)
	{
		registry.Destroy(entity);
	}
}

} // namespace openblack::ecs::learning_marker
