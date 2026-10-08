/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MapCoords.h"

#include "3D/LandIslandInterface.h"
#include "Locator.h"

using namespace openblack;

namespace
{
/// The ground height at the MapCoords' own (truncated) position: the island's GetHeightAt
float GroundAt(const LandIslandInterface* island, const map_coords::MapCoords& coords)
{
	return island != nullptr ? island->GetHeightAt(map_coords::ToMetres(coords)) : 0.0f;
}

const LandIslandInterface* Island()
{
	return Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
}
} // namespace

namespace openblack::map_coords
{

MapCoords FromWorld(const LandIslandInterface* island, glm::vec3 point)
{
	MapCoords coords {ToFixed(point.x), ToFixed(point.z), 0.0f};
	coords.altitude = point.y - GroundAt(island, coords); // y - the ground
	return coords;
}

glm::vec3 ToWorld(const LandIslandInterface* island, const MapCoords& coords)
{
	return {ToMetres(coords.x), GroundAt(island, coords) + coords.altitude, ToMetres(coords.z)};
}

MapCoords FromWorld(glm::vec3 point)
{
	return FromWorld(Island(), point);
}

glm::vec3 ToWorld(const MapCoords& coords)
{
	return ToWorld(Island(), coords);
}

} // namespace openblack::map_coords
