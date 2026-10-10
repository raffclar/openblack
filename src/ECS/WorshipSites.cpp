/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorshipSites.h"

#include <cmath>

#include <glm/geometric.hpp>

namespace openblack::ecs::worship_site
{

float PlaceFacing(float templeFacing, uint32_t place)
{
	return templeFacing + static_cast<float>(place) * k_PlaceSpacing;
}

glm::vec2 TurnedPoint(glm::vec2 spot, float facing, glm::vec3 point)
{
	const float cosine = std::cos(facing);
	const float sine = std::sin(facing);
	return spot + glm::vec2(cosine * point.x - sine * point.z, sine * point.x + cosine * point.z);
}

std::optional<uint32_t> NearestFreePlace(const std::array<bool, k_Places>& taken, glm::vec2 temple, glm::vec3 placePoint,
                                         glm::vec2 spot)
{
	// Nearer than this or no place at all, the first of equally near places kept
	float nearest = 999999.0f;
	std::optional<uint32_t> best;
	for (uint32_t place = 0; place < k_Places; ++place)
	{
		if (taken.at(place))
		{
			continue;
		}
		const float distance = glm::distance(spot, TurnedPoint(temple, PlaceFacing(0.0f, place), placePoint));
		if (distance < nearest)
		{
			nearest = distance;
			best = place;
		}
	}
	return best;
}

bool MayHaveSite(int32_t landNumber, bool stoppedByScript, uint32_t population)
{
	return landNumber != k_LandWithoutSites && !stoppedByScript && population != 0;
}

} // namespace openblack::ecs::worship_site
