/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FloatingNumber.h"

#include <vector>

#include "ECS/Components/FloatingNumber.h"
#include "ECS/Registry.h"

namespace openblack::ecs::floating_number
{

bool Step(components::FloatingNumber& number, float seconds)
{
	number.position.y += seconds * k_RiseSpeed;
	number.life -= seconds;
	return number.life >= 0.0f;
}

std::optional<uint8_t> Alpha(float life)
{
	// Worked wider than a float, as the game does
	const auto alpha = static_cast<int32_t>(static_cast<double>(life) * 255.0);
	if (alpha < 1)
	{
		return std::nullopt;
	}
	return static_cast<uint8_t>(alpha > 0xFF ? 0xFF : alpha);
}

glm::vec2 TextPlace(glm::vec2 screenPoint, float textWidth)
{
	return {screenPoint.x - textWidth, screenPoint.y};
}

void StepAll(Registry& registry, float seconds)
{
	std::vector<entt::entity> gone;
	registry.Each<components::FloatingNumber>([seconds, &gone](entt::entity entity, components::FloatingNumber& number) {
		if (!Step(number, seconds))
		{
			gone.push_back(entity);
		}
	});
	for (const auto entity : gone)
	{
		registry.Destroy(entity);
	}
}

} // namespace openblack::ecs::floating_number
