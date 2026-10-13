/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSway.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

namespace openblack::creature_sway
{

void Kick(Sway& sway, glm::vec3 force, float heightAboveLand, float splitHeight, float mass)
{
	auto& velocity = heightAboveLand > splitHeight ? sway.upperVelocity : sway.lowerVelocity;
	const float gain = sway.frameSeconds / (mass * 0.5f);
	velocity.x += gain * force.x;
	velocity.y = 0.0f;
	velocity.z += gain * force.z;
	const float speed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
	if (speed > k_MaxKickSpeed)
	{
		velocity *= k_MaxKickSpeed / speed;
	}
	sway.active = true;
}

void Step(Sway& sway, float seconds, float mass)
{
	if (!sway.active)
	{
		return;
	}
	const float step = seconds * (1.0f / static_cast<float>(k_Substeps));
	const float halfMass = mass * 0.5f;
	const float stiffness = halfMass * k_Stiffness;
	const float damping = halfMass * k_Damping;
	const float gain = step / halfMass;
	bool settled = true;
	for (auto [velocity, offset] :
	     {std::pair {&sway.lowerVelocity, &sway.lowerOffset}, std::pair {&sway.upperVelocity, &sway.upperOffset}})
	{
		for (int i = 0; i < k_Substeps; ++i)
		{
			const auto push = stiffness * *offset;
			// The speed is kept within its limit before the spring acts on it
			const float speedSquared = glm::dot(*velocity, *velocity);
			if (speedSquared > k_MaxSwingSpeed * k_MaxSwingSpeed)
			{
				*velocity *= k_MaxSwingSpeed / std::sqrt(speedSquared);
			}
			*velocity += (push - damping * *velocity) * gain;
			*offset += step * *velocity;
		}
		if (glm::length(*offset) > k_SettledLength || glm::length(*velocity) > k_SettledLength)
		{
			settled = false;
		}
	}
	if (settled)
	{
		sway.active = false;
	}
}

uint32_t LeanTime(float lean, uint32_t duration)
{
	const float share = (std::clamp(lean, -1.0f, 1.0f) + 1.0f) * 0.5f;
	return static_cast<uint32_t>(share * static_cast<float>(duration));
}
} // namespace openblack::creature_sway
