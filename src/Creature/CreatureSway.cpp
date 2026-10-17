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

#include "3D/MapCoords.h"
#include "Creature/CreatureLocomotion.h"

namespace openblack::creature_sway
{

namespace
{
/// The length squared of a vector, summed from the last axis to the first
float LengthSquared(glm::vec3 v)
{
	return ((v.z * v.z) + (v.y * v.y)) + (v.x * v.x);
}
} // namespace

float TurnSeconds(float pace)
{
	const auto milliseconds = map_coords::FtoL(pace * static_cast<float>(k_TurnMilliseconds));
	return static_cast<float>(milliseconds) * 0.001f;
}

GameHeading TurnHeading(GameHeading heading, GameHeading targetHeading, float timeMs, float durationMs)
{
	return GameHeading(((targetHeading.TurnFrom(heading) * timeMs) / durationMs) + heading.Radians());
}

void Kick(Sway& sway, glm::vec3 force, float heightAboveLand, float splitHeight, float mass)
{
	auto& velocity = heightAboveLand > splitHeight ? sway.upperVelocity : sway.lowerVelocity;
	const float gain = sway.turnSeconds / (mass * 0.5f);
	velocity.x = (gain * force.x) + velocity.x;
	velocity.y = 0.0f;
	velocity.z = (gain * force.z) + velocity.z;
	const float speed = std::sqrt((velocity.z * velocity.z) + (velocity.x * velocity.x));
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
			const float speedSquared = LengthSquared(*velocity);
			if (speedSquared > k_MaxSwingSpeed * k_MaxSwingSpeed)
			{
				*velocity *= k_MaxSwingSpeed / std::sqrt(speedSquared);
			}
			*velocity = ((push - damping * *velocity) * gain) + *velocity;
			*offset = (step * *velocity) + *offset;
		}
		if (std::sqrt(LengthSquared(*offset)) > k_SettledLength || std::sqrt(LengthSquared(*velocity)) > k_SettledLength)
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
