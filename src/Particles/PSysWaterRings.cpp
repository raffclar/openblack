/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PSysWaterRings.h"

#include "ECS/SeaCells.h"
#include "ECS/WaterRings.h"

namespace openblack::psys::water_rings
{
namespace
{
/// The fields both creators write: age 0, growth, angle 0, aspect = rate = 1, cell 0x30, colour 0xFFFFFFFF. The drift
/// is left as the slot had it, see WaterRing.
bool AddRing(const glm::vec3& position, float growth)
{
	const ecs::WaterRing ring {.position = position,
	                           .age = 0,
	                           .growth = growth,
	                           .angle = 0.0f,
	                           .aspect = 1.0f,
	                           .rate = 1.0f,
	                           .cell = 0x30,
	                           .argb = 0xFFFFFFFFu};
	return ecs::AddWaterRing(ring);
}
} // namespace

bool AddExplosionRings(const glm::vec3& point)
{
	if (ecs::sea_cells::IsDryLand(point))
	{
		return false;
	}
	AddRing(point, k_ExplosionRingGrowth * 0.5f);
	AddRing(point, k_ExplosionRingGrowth * 0.7f);
	AddRing(point, k_ExplosionRingGrowth);
	return true;
}

bool AddParticleRipple(const glm::vec3& position, float atomRadius, glm::vec3& lastRipple, float minDistance)
{
	if (!ecs::sea_cells::IsWater(position))
	{
		return false;
	}
	// minDistance^2 < dx^2 + dz^2
	const float dx = lastRipple.x - position.x;
	const float dz = lastRipple.z - position.z;
	if (!(minDistance * minDistance < dx * dx + dz * dz))
	{
		return false;
	}
	lastRipple = position;
	return AddRing(position, atomRadius * 4.0f);
}

} // namespace openblack::psys::water_rings
