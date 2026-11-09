/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleGround.h"

#include <cmath>

#include "Common/X87.h"

using namespace openblack;

namespace
{
constexpr float k_CellSize = 10.0f;
constexpr float k_CellsPerMetre = 0.1f;
} // namespace

float temple_ground::OwnShare(int dx, int dz)
{
	// The squares of whole numbers of metres are exact, so only the square root rounds
	const float x = static_cast<float>(dx) * k_CellSize;
	const float z = static_cast<float>(dz) * k_CellSize;
	const float distance = std::sqrt(z * z + x * x);
	if (distance > k_EdgeRadius)
	{
		return 1.0f;
	}
	if (distance < k_FlatRadius)
	{
		return 0.0f;
	}
	return (distance - k_FlatRadius) * k_SharePerMetre;
}

uint8_t temple_ground::LevelledAltitude(uint8_t own, float ownShare, uint8_t centre)
{
	const float kept = static_cast<float>(own) * ownShare;
	const float given = (1.0f - ownShare) * static_cast<float>(centre);
	return static_cast<uint8_t>(x87::ToInteger(kept + given));
}

glm::ivec2 temple_ground::CentreCorner(const map_coords::MapCoords& place)
{
	return {x87::ToInteger(map_coords::ToMetres(place.x) * k_CellsPerMetre),
	        x87::ToInteger(map_coords::ToMetres(place.z) * k_CellsPerMetre)};
}

std::vector<temple_ground::CornerAltitude> temple_ground::Levelled(glm::ivec2 centre,
                                                                   const std::function<uint8_t(glm::ivec2)>& altitudeAt)
{
	// The game reads the centre altitude one time, before it changes a corner
	const auto centreAltitude = altitudeAt(centre);
	std::vector<CornerAltitude> corners;
	corners.reserve(static_cast<size_t>((2 * k_Reach + 1) * (2 * k_Reach + 1)));
	for (int x = centre.x - k_Reach; x <= centre.x + k_Reach; ++x)
	{
		for (int z = centre.y - k_Reach; z <= centre.y + k_Reach; ++z)
		{
			const glm::ivec2 corner {x, z};
			const float share = OwnShare(x - centre.x, z - centre.y);
			corners.push_back({.corner = corner, .altitude = LevelledAltitude(altitudeAt(corner), share, centreAltitude)});
		}
	}
	return corners;
}
