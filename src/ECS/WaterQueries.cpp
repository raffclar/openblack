/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WaterQueries.h"

#include <cstdint>

#include <glm/vec2.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/Stream.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "Locator.h"

using namespace openblack;

namespace
{
namespace map_coords = openblack::map_coords;
using map_coords::MapCoords;
/// A point to MapCoords: x and z scaled to fixed point and truncated; `altitude` carries the point's y as is
MapCoords FromPoint(glm::vec3 point)
{
	return {map_coords::ToFixed(point.x), map_coords::ToFixed(point.z), point.y};
}

glm::vec3 ToPoint(const MapCoords& coords)
{
	return {map_coords::ToMetres(coords.x), coords.altitude, map_coords::ToMetres(coords.z)};
}

/// The cell is inside the game map
bool InBounds(const LandIslandInterface& island, const MapCoords& coords)
{
	return map_coords::InBounds(coords, island.GetCellsPerSide());
}

std::optional<MapCoords> NearestCoastal(const LandIslandInterface& island, const MapCoords& from, float radius)
{
	MapCoords coords = from;
	map_coords::Spiral spiral;
	for (int32_t steps = 999999; steps != 0;)
	{
		// stop once farther than the radius (or unordered)
		if (!(gutils::GetDistanceInMetres(from, coords) <= radius))
		{
			return std::nullopt;
		}
		if (InBounds(island, coords) && ecs::sea_cells::IsCoastal(island, map_coords::Cell(coords)))
		{
			return coords;
		}
		--steps;
		map_coords::AddCells(coords, spiral.Next());
	}
	return std::nullopt;
}

std::optional<MapCoords> NearestStreamPos(const LandIslandInterface& island, const MapCoords& from, float radius)
{
	// the world point of `from` (its y, the ground's altitude + from.y, is not used by the xz distance)
	const auto x = map_coords::ToMetres(from.x);
	const auto z = map_coords::ToMetres(from.z);
	std::optional<glm::vec3> best;
	// the game's streams (newest first: each new stream goes to the head of the list) and their points (script
	// order). entt walks a storage from its last element to its first, so the
	// view also gives the newest stream first (streams are only removed all together, with the map). The order only
	// matters for two points at exactly the same distance: the first one found wins.
	Locator::entitiesRegistry::value().Each<const ecs::components::Stream>([&](const ecs::components::Stream& stream) {
		for (const auto& point : stream.points)
		{
			// the xz distance, the differences stored as floats
			const float distance = gutils::Hypotenuse(point.x - x, point.z - z);
			if (distance < radius) // strictly closer
			{
				radius = distance; // stored as a float
				best = point;
			}
		}
	});
	if (!best)
	{
		return std::nullopt;
	}
	MapCoords out {map_coords::ToFixed(best->x), map_coords::ToFixed(best->z), 0.0f};
	// y: the point's altitude above the ground at MapCoords(x * 65536 * 0.1, z * 65536 * 0.1): (x * 2^16) * 0.1f is
	// rounded like x * 6553.6f (= 0.1f * 2^16), so the same MapCoords
	out.altitude = best->y - island.GetHeightAt(map_coords::ToMetres(out));
	return out;
}

bool NearestDrinkingWater(const LandIslandInterface& island, const MapCoords& from, MapCoords& out, float radius)
{
	if (const auto river = NearestStreamPos(island, from, radius))
	{
		out = *river;
		// a coast no farther than the river (from `from`, not from the river point)
		const float riverDistance = gutils::GetDistanceInMetres(from, out); // stored as a float
		if (const auto coast = NearestCoastal(island, from, riverDistance))
		{
			out = *coast;
		}
		return true; // found, whatever the coast search says
	}
	if (const auto coast = NearestCoastal(island, from, radius))
	{
		out = *coast;
		return true;
	}
	return false;
}

const LandIslandInterface* Island()
{
	return Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
}
} // namespace

namespace openblack::ecs::water_queries
{

float GetDistanceInMetres(glm::vec3 a, glm::vec3 b)
{
	return gutils::GetDistanceInMetres(FromPoint(a), FromPoint(b));
}

std::optional<glm::vec3> FindNearestCoastalTo(const LandIslandInterface& island, glm::vec3 from, float radius)
{
	const auto coords = NearestCoastal(island, FromPoint(from), radius);
	return coords ? std::optional(ToPoint(*coords)) : std::nullopt;
}

std::optional<glm::vec3> FindNearestStreamPosTo(const LandIslandInterface& island, glm::vec3 from, float radius)
{
	const auto coords = NearestStreamPos(island, FromPoint(from), radius);
	return coords ? std::optional(ToPoint(*coords)) : std::nullopt;
}

bool FindNearestDrinkingWater(const LandIslandInterface& island, glm::vec3 from, glm::vec3& out, float radius)
{
	MapCoords coords = FromPoint(out);
	if (!NearestDrinkingWater(island, FromPoint(from), coords, radius))
	{
		return false;
	}
	out = ToPoint(coords);
	return true;
}

bool FindNearestDrinkingWater(const LandIslandInterface& island, DrinkingWater& water, glm::vec3 abodePosition, float radius)
{
	// the flag takes the answer, the position changes only where water was found
	water.found = FindNearestDrinkingWater(island, abodePosition, water.position, radius);
	return water.found;
}

std::optional<glm::vec3> GetNearestWaterPos(const DrinkingWater& water)
{
	return water.found ? std::optional(water.position) : std::nullopt;
}

std::optional<glm::vec3> FindNearestCoastalTo(glm::vec3 from, float radius)
{
	const auto* island = Island();
	return island != nullptr ? FindNearestCoastalTo(*island, from, radius) : std::nullopt;
}

std::optional<glm::vec3> FindNearestStreamPosTo(glm::vec3 from, float radius)
{
	const auto* island = Island();
	return island != nullptr ? FindNearestStreamPosTo(*island, from, radius) : std::nullopt;
}

bool FindNearestDrinkingWater(glm::vec3 from, glm::vec3& out, float radius)
{
	const auto* island = Island();
	return island != nullptr && FindNearestDrinkingWater(*island, from, out, radius);
}

} // namespace openblack::ecs::water_queries
