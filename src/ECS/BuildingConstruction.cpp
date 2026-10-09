/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BuildingConstruction.h"

#include <algorithm>

#include <glm/geometric.hpp>

namespace openblack::building_construction
{

Progress SetBuilt(float built)
{
	const float clamped = built < 0.0f ? 0.0f : built;
	if (clamped >= 1.0f)
	{
		return {.built = 1.0f, .finished = true};
	}
	return {.built = clamped, .finished = false};
}

Progress BuildBy(float built, float amount)
{
	return SetBuilt(built + amount);
}

std::optional<std::size_t> PlannedAt(std::span<const PlannedCandidate> planned, glm::vec2 place, float radius)
{
	std::optional<std::size_t> best;
	float bestDistance = radius;
	for (std::size_t i = 0; i < planned.size(); ++i)
	{
		const float distance = glm::distance(place, planned[i].position) - (planned[i].reach + radius);
		if (distance <= bestDistance)
		{
			bestDistance = distance;
			best = i;
		}
	}
	return best;
}

float ModelReach(glm::vec2 halfWidths, float scale)
{
	return std::max(halfWidths.x, halfWidths.y) * scale;
}

DrawnBuild FollowBuilt(float drawn, float built)
{
	const float shown = std::clamp(built, 0.0f, 1.0f);
	return {.drawn = shown, .refile = drawn < 1.0f && shown >= 1.0f};
}

bool PlaysFinishedMusic(const FinishedTemple& temple)
{
	return temple.localPlayers && !temple.scriptCutScene && !temple.scriptMusic && temple.landNumber != k_SilentFinishLand;
}

} // namespace openblack::building_construction
