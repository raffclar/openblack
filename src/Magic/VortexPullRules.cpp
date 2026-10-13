/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VortexPullRules.h"

#include <algorithm>

#include "Common/GUtilsDistance.h"

namespace openblack::vortex
{

float PullReach(uint32_t turn, float radius)
{
	constexpr uint32_t k_FullReachEvery = 3;
	constexpr float k_ShortReach = 0.5f;
	return turn % k_FullReachEvery == 0 ? radius : radius * k_ShortReach;
}

std::vector<glm::ivec2> CellsSearched(const map_coords::MapCoords& centre, float reach)
{
	std::vector<glm::ivec2> cells;
	auto at = centre;
	map_coords::Spiral spiral;
	for (int32_t left = k_MostCellsSearched; left > 0; --left)
	{
		const glm::ivec2 cell = map_coords::Cell(at);
		if (map_coords::InBounds(cell))
		{
			if (reach < gutils::GetDistanceInMetres(at, centre))
			{
				break;
			}
			cells.push_back(cell);
		}
		const auto& step = spiral.Next();
		at.x += step.x * map_coords::k_FixedPerCell;
		at.z += step.z * map_coords::k_FixedPerCell;
	}
	return cells;
}

bool CreatureMayBeTaken(bool letThrough, bool leashed, bool movedByScript)
{
	return (letThrough && leashed) || movedByScript;
}

float CreatureTakenDistance(float modelReachX, float modelReachZ, float scale)
{
	return std::max(modelReachX, modelReachZ) * scale * k_CreatureTakenShare;
}

bool TakesWhatStrikesIt(VortexType type, VortexStateType state, bool hitterFlying, bool hitterMayBeTaken)
{
	return type == VortexType::In && state == VortexStateType::Active && hitterFlying && hitterMayBeTaken;
}

} // namespace openblack::vortex
