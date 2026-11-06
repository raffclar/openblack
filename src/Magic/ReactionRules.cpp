/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ReactionRules.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "Common/X87Vector.h"

using namespace openblack::magic;

float openblack::magic::FastMapDistance(glm::vec3 a, glm::vec3 b)
{
	const float dx = std::abs(a.x - b.x) * k_MapUnitsPerMetre;
	const float dz = std::abs(a.z - b.z) * k_MapUnitsPerMetre;
	return std::max(dx, dz) + std::min(dx, dz) * 0.5f;
}

uint32_t openblack::magic::KindPriority(Reaction type, uint32_t tablePriority, float fastDistance, bool isCaster)
{
	switch (type)
	{
	case Reaction::FleeFromSpell:
		// A creature never runs from a miracle it cast itself
		if (isCaster)
		{
			return 0;
		}
		// The game keeps this sum in one byte. The game's table gives 150 here, so the sum is at most 250 and the byte
		// does not overflow.
		if (fastDistance < k_FleeUrgencyUnits)
		{
			return tablePriority + static_cast<uint32_t>(100.0f * (k_FleeUrgencyUnits - fastDistance) / k_FleeUrgencyUnits);
		}
		return tablePriority;
	case Reaction::LookAtNiceSpell:
	case Reaction::ReactToImpressiveSpell:
		return isCaster ? 0 : tablePriority;
	default:
		return tablePriority;
	}
}

bool openblack::magic::ChangesReaction(Reaction current, uint32_t currentPriority, uint32_t nextPriority,
                                       uint32_t secondsReacting)
{
	if (!(currentPriority < nextPriority))
	{
		return false;
	}
	const auto least =
	    current == Reaction::ReactToHandPickUp ? k_ReactSecondsBeforeChangingForHand : k_ReactSecondsBeforeChanging;
	return secondsReacting >= least;
}

bool openblack::magic::Moves(glm::vec3 velocity)
{
	const float lengthSquared = (velocity.y * velocity.y + velocity.z * velocity.z) + velocity.x * velocity.x;
	return std::sqrt(lengthSquared) > 0.0f;
}

glm::vec2 openblack::magic::FleePointFromStill(const map_coords::MapCoords& living, const map_coords::MapCoords& thing,
                                               float run)
{
	const auto here = map_coords::ToMetres(living);
	const auto there = map_coords::ToMetres(thing);
	float x = here.x - there.x;
	float z = here.y - there.y;
	if (x != 0.0f || z != 0.0f)
	{
		const float scale = run / std::sqrt(z * z + x * x);
		x = x * scale;
		z = z * scale;
	}
	return {x + here.x, z + here.y};
}

openblack::map_coords::MapCoords openblack::magic::FleePlaceFromMoving(const map_coords::MapCoords& living,
                                                                       const map_coords::MapCoords& thing, glm::vec3 velocity,
                                                                       float run, glm::vec2 random)
{
	const auto here = map_coords::ToMetres(living);
	const auto there = map_coords::ToMetres(thing);
	const float awayX = here.x - there.x;
	const float awayZ = here.y - there.y;
	// Across the way of the thing. The height of this direction is 0.
	float acrossX = -velocity.z;
	float acrossZ = velocity.x;
	if (acrossX != 0.0f || acrossZ != 0.0f)
	{
		const float inverse = 1.0f / std::sqrt((0.0f + acrossX * acrossX) + acrossZ * acrossZ);
		acrossX = inverse * acrossX;
		acrossZ = inverse * acrossZ;
	}
	const float half = k_FleeJitter * 0.5f;
	float x = (random.x + acrossX * run) - half;
	float z = (random.y + acrossZ * run) - half;
	// The living thing is on the side of the way that the across direction points to, or on the other side
	if (acrossZ * awayZ + acrossX * awayX < 0.0f)
	{
		x = -x;
		z = -z;
	}
	return {
	    .x = map_coords::ToFixedGUtils(here.x + x), .z = map_coords::ToFixedGUtils(here.y + z), .altitude = living.altitude};
}

bool openblack::magic::ComingTowards(glm::vec3 villager, glm::vec3 object, glm::vec3 velocity)
{
	const auto toVillager = villager - object;
	if (glm::length(toVillager) == 0.0f || glm::length(velocity) == 0.0f)
	{
		return false;
	}
	// The game multiplies each by one over its length, and adds every sum from z to x
	using openblack::x87::SumOrder;
	return !(openblack::x87::DotZyx(openblack::x87::Scaled<SumOrder::Zyx>(toVillager),
	                                openblack::x87::Scaled<SumOrder::Zyx>(velocity)) < k_ComingTowardsCosine);
}

bool openblack::magic::TimesOut(openblack::Reaction type)
{
	using openblack::Reaction;
	switch (type)
	{
	case Reaction::LookAtObject:
	case Reaction::ReactToNewBuilding:
	case Reaction::ReactToHandPickUp:
	case Reaction::ReactToHandUsingTotem:
	case Reaction::ReactToObjectCrushed:
	case Reaction::ReactToFight:
	case Reaction::LookAtNiceSpell:
	case Reaction::ReactToHandPuttingStuffInStoragePit:
	case Reaction::ReactToDeath:
	case Reaction::ReactToDroppedByHand:
	case Reaction::Fainting:
	case Reaction::Confused:
	case Reaction::AvoidFallingTree:
	case Reaction::CrowdAround:
	case Reaction::ReactToTownCelebration:
	case Reaction::ReactToVillagerInHand:
	case Reaction::ReactToMagicWaterPuttingOutFire:
	case Reaction::ReactToMagicShieldStruck:
	case Reaction::ReactToScaffold:
	case Reaction::ReactToFightWon:
		return true;
	default:
		return false;
	}
}

bool openblack::magic::TimedOut(openblack::Reaction type, std::optional<uint32_t> stamp, uint32_t age, uint32_t turns)
{
	return TimesOut(type) && stamp.has_value() && age - *stamp > turns;
}

float openblack::magic::StartingReach(bool grows, float maxDistance)
{
	return grows ? 1.0f : maxDistance;
}
