/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleHeart.h"

#include <cmath>

#include <optional>

namespace openblack::physics::temple_heart
{

Target Choose(std::span<const Town> towns)
{
	std::optional<entt::entity> first;
	for (const auto& town : towns)
	{
		for (const auto& building : town.buildings)
		{
			if (!building.available || !(building.life > 0.0f) || !(building.built > 0.0f) || building.field ||
			    building.footballPitch)
			{
				continue;
			}
			if (building.life > k_SoundLife)
			{
				return {.kind = TargetKind::Building, .entity = building.entity};
			}
			if (!first.has_value())
			{
				first = building.entity;
			}
		}
	}
	if (first.has_value())
	{
		return {.kind = TargetKind::Building, .entity = *first};
	}
	for (const auto& town : towns)
	{
		for (const auto& villager : town.homeless)
		{
			if (villager.available)
			{
				return {.kind = TargetKind::Villager, .entity = villager.entity};
			}
		}
	}
	return {};
}

float Harm(glm::vec3 velocity, float mass)
{
	// Worked in wider precision, z and y first, and rounded once
	const double x = velocity.x;
	const double y = velocity.y;
	const double z = velocity.z;
	const auto harm = static_cast<float>(std::sqrt(((z * z) + (y * y)) + (x * x)) * static_cast<double>(mass) *
	                                     static_cast<double>(k_HarmPerMomentum));
	return k_MostHarm < harm ? k_MostHarm : harm;
}

uint32_t BeamInterval(uint32_t millisecondsPerTurn)
{
	// Turns a second as a whole number, times two, cut down to a whole number
	constexpr double k_Seconds = 2.0;
	return static_cast<uint32_t>(static_cast<double>(1000u / millisecondsPerTurn) * k_Seconds);
}

bool BeamAtTargetDue(Beam& beam, entt::entity target, uint32_t turn, uint32_t interval)
{
	if (target != beam.target)
	{
		beam.target = target;
		beam.turn = 0;
	}
	if (beam.target == entt::null || !(interval + beam.turn < turn))
	{
		return false;
	}
	beam.turn = turn;
	return true;
}

bool BeamAtItselfDue(Beam& beam, uint32_t turn, uint32_t interval)
{
	if (beam.target != entt::null)
	{
		beam.target = entt::null;
		beam.turn = 0;
	}
	if (!(interval + beam.turn < turn))
	{
		return false;
	}
	beam.turn = turn;
	return true;
}

} // namespace openblack::physics::temple_heart
