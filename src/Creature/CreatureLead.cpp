/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureLead.h"

#include <cmath>

#include <algorithm>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/X87.h"

using namespace openblack;
using namespace openblack::creature_lead;

namespace
{
/// The speed that a living thing keeps, times this and then times one over 65536, is in metres a second
constexpr float k_SpeedScale = 100.0f;
constexpr float k_OneOver65536 = 1.52587890625e-05f;
/// The game keeps this factor of the reach time as a double
constexpr double k_MillisecondsToSeconds = 0.001;

/// The distance along the ground from the creature to a place, with the square of z first
float FlatDistance(const glm::vec3& place, const glm::vec3& creature)
{
	const float dx = creature.x - place.x;
	const float dz = creature.z - place.z;
	return std::sqrt((dz * dz) + (dx * dx));
}
} // namespace

float creature_lead::SpeedOf(uint16_t speed)
{
	return (static_cast<float>(speed) * k_SpeedScale) * k_OneOver65536;
}

glm::vec3 creature_lead::PlaceOf(const Living& living)
{
	return {map_coords::ToMetres(living.mapPosition.x), living.height, map_coords::ToMetres(living.mapPosition.y)};
}

creature_lead::Living creature_lead::CreatureAsLiving(glm::ivec2 mapPosition, float height, int32_t tableSpeed, bool onItsWay)
{
	constexpr int32_t k_MostSpeed = 0xFFFF;
	return {.mapPosition = mapPosition,
	        .height = height,
	        .speed = static_cast<uint16_t>(std::clamp(tableSpeed, 0, k_MostSpeed)),
	        .gameAngle = 0,
	        .moved = onItsWay};
}

glm::vec3 creature_lead::WhereAfter(const Living& living, float seconds)
{
	auto place = PlaceOf(living);
	const float speed = SpeedOf(living.speed);
	if (!(speed > k_LeastLeadSpeed))
	{
		return place;
	}
	// It continues along the way that it faces. The height stays.
	const float angle = gutils::ConvertGameAngleToScawenAngle(living.gameAngle);
	const float distance = speed * seconds;
	const float acrossX = x87::Mul(x87::Sin(angle), distance);
	const float acrossZ = -x87::Mul(x87::Cos(angle), distance);
	place.x = acrossX + place.x;
	place.z = acrossZ + place.z;
	return place;
}

glm::vec3 creature_lead::WalkTo(const Walker& walker, const Living& living)
{
	const float speed = walker.runSpeed * walker.fraction;
	const float most = walker.size * k_MostWalkPerSize;
	const float stop = (speed * speed) / (k_Acceleration + k_Acceleration);
	const auto walkSeconds = [&](float distance, float stopDistance) {
		return ((distance - stopDistance) / speed) + (speed / k_Acceleration);
	};

	// The first time comes from where the thing is now. The distance to stop stays the same for the second time.
	const float distance = FlatDistance(PlaceOf(living), walker.position);
	const float counted = distance < most ? distance : most;
	const float stopDistance = counted < stop ? counted : stop;
	const auto first = WhereAfter(living, walkSeconds(counted, stopDistance));

	const float secondDistance = FlatDistance(first, walker.position);
	const float secondCounted = secondDistance < most ? secondDistance : most;
	return WhereAfter(living, walkSeconds(secondCounted, stopDistance));
}

float creature_lead::ReachSeconds(float size, int32_t milliseconds)
{
	const float root = x87::Sqrt(size + size);
	return x87::Mul(x87::Mul(root, k_MillisecondsToSeconds), static_cast<double>(milliseconds));
}
