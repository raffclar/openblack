/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptPropertyRules.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include <glm/gtx/euler_angles.hpp>

#include "3D/MapCoords.h"

namespace openblack::script::property_rules
{

bool TownCompletelyDestroyed(std::span<const TownBuilding> buildings)
{
	return std::ranges::none_of(buildings, [](const TownBuilding& building) {
		return building.life > 0.0f && !building.field && (building.built >= 1.0f || building.built > k_StandingBuilt);
	});
}

float PlayerProperty(std::optional<PlayerNames> player, bool destroyedTown)
{
	if (destroyedTown)
	{
		return 1.0f;
	}
	if (!player.has_value() || *player == PlayerNames::NEUTRAL)
	{
		return 0.0f;
	}
	return static_cast<float>(static_cast<uint32_t>(*player) + 1);
}

bool InCreatureHand(entt::entity thing, entt::entity carried, std::optional<entt::entity> eating)
{
	return thing != entt::null && (carried == thing || eating == thing);
}

namespace
{
/// Below the floor gives the floor, up to 1 is kept, anything else (above 1, or not a number) gives 1
float KeepBetween(float value, float floor)
{
	if (value < floor)
	{
		return floor;
	}
	return value <= 1.0f ? value : 1.0f;
}
} // namespace

float SetNeed(CreatureNeed need, float value)
{
	switch (need)
	{
	case CreatureNeed::Warmth:
		return KeepBetween(value, -1.0f);
	case CreatureNeed::Energy:
		return KeepBetween(value, 0.0f);
	default:
		return value;
	}
}

float AngleToScript(float radians)
{
	return radians * k_RadiansToTurns * k_DegreesInATurn;
}

float AngleFromScript(float degrees)
{
	return degrees * k_DegreesToRadians;
}

float FacingAngle(glm::vec2 from, glm::vec2 to)
{
	// Worked out at the wider precision before it is kept as a float
	double angle = std::atan2(static_cast<double>(to.y) - static_cast<double>(from.y),
	                          static_cast<double>(to.x) - static_cast<double>(from.x));
	if (angle < 0.0)
	{
		angle += static_cast<double>(k_WholeTurnRadians);
	}
	return static_cast<float>(angle);
}

bool CanSetLife(float life, bool heldDuringCutscene, bool indestructible)
{
	return !((heldDuringCutscene || indestructible) && life <= k_LowestProtectedLife);
}

float CreatureHeight(float size)
{
	return size * k_CreatureHeightPerSize;
}

float CreatureSizeForHeight(float height)
{
	return height * k_CreatureSizePerHeight;
}

Angles PlacedAngles(const glm::mat3& rotation)
{
	// Upright: only turned about the upright axis, which is read whole
	constexpr float k_Upright = 1e-6f;
	if (std::abs(rotation[1][1] - 1.0f) < k_Upright)
	{
		return {.x = 0.0f, .y = -std::atan2(rotation[2][0], rotation[0][0]), .z = 0.0f};
	}
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	glm::extractEulerAngleXYZ(glm::mat4(rotation), x, y, z);
	return {.x = -x, .y = -y, .z = -z};
}

glm::mat3 PlacedRotation(const Angles& angles)
{
	return glm::mat3(glm::eulerAngleXYZ(-angles.x, -angles.y, -angles.z));
}

float CreatureHeadingToLivingAngle(float heading)
{
	constexpr float k_Turn = 2.0f * std::numbers::pi_v<float>;
	const float angle = std::fmod(-heading - (std::numbers::pi_v<float> * 0.5f), k_Turn);
	return angle < 0.0f ? angle + k_Turn : angle;
}

float LivingAngleToCreatureHeading(float angle)
{
	return -angle - (std::numbers::pi_v<float> * 0.5f);
}

bool MovedAcross(const glm::vec3& before, const glm::vec3& after)
{
	return map_coords::ToFixed(before.x) != map_coords::ToFixed(after.x) ||
	       map_coords::ToFixed(before.z) != map_coords::ToFixed(after.z);
}

std::optional<CreatureType> CreatureTypeFromScript(uint32_t type)
{
	constexpr uint32_t k_GiantApe = 0;
	if (type == k_GiantApe)
	{
		return CreatureType::GiantApe;
	}
	if (type < static_cast<uint32_t>(CreatureType::GiantApe))
	{
		return static_cast<CreatureType>(type);
	}
	return std::nullopt;
}

uint32_t ScriptCreatureType(CreatureType species)
{
	return species == CreatureType::GiantApe ? 0 : static_cast<uint32_t>(species);
}

float BeliefForPlayer(bool town, std::optional<float> townBelief, std::optional<PlayerNames> owner, PlayerNames player)
{
	if (town)
	{
		return townBelief.value_or(0.0f);
	}
	return owner == player ? 1.0f : 0.0f;
}

float CreatureSpeedFraction(float speed)
{
	return std::clamp(speed, 0.0f, 1.0f);
}

CopiedBody CopyBody(float size, float strength, float scale)
{
	return {.size = size * scale, .strength = strength * scale};
}

std::optional<PlayerNames> OwnerOfCopy(std::span<const bool> playersHaveCreatures)
{
	constexpr size_t k_Players = 7;
	for (size_t i = 0; i < k_Players && i < playersHaveCreatures.size(); ++i)
	{
		if (!playersHaveCreatures[i])
		{
			return static_cast<PlayerNames>(i);
		}
	}
	return std::nullopt;
}

glm::vec3 HeadingVelocity(const glm::vec3& from, const glm::vec3& to, float speed)
{
	const glm::vec3 delta = to - from;
	if (delta == glm::vec3(0.0f))
	{
		return glm::vec3(0.0f);
	}
	// The speed over the length is worked at the higher precision and rounded to a float, then each part is scaled by it
	const double dx = delta.x;
	const double dy = delta.y;
	const double dz = delta.z;
	const auto scale = static_cast<float>(static_cast<double>(speed) / std::sqrt(dx * dx + dy * dy + dz * dz));
	return {scale * delta.x, delta.y * scale, delta.z * scale};
}

float HeadingSpin(float draw)
{
	// Worked at double precision and only then rounded to a float
	constexpr double k_Middle = 100.0;
	constexpr double k_Scale = 0.01;
	return static_cast<float>((static_cast<double>(draw) - k_Middle) * k_Scale);
}

} // namespace openblack::script::property_rules
