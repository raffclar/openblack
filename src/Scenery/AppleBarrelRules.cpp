/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AppleBarrelRules.h"

#include <algorithm>

#include <glm/geometric.hpp>

using namespace openblack;

glm::vec2 apple_barrel::MeasuredFrom(std::optional<glm::vec2> temple)
{
	return temple.value_or(k_NoTemplePlace);
}

std::optional<size_t> apple_barrel::NearestStore(std::span<const glm::vec2> stores, glm::vec2 from)
{
	std::optional<size_t> nearest;
	float nearestDistance = k_FarthestStore;
	for (size_t i = 0; i < stores.size(); ++i)
	{
		const float distance = glm::distance(stores[i], from);
		if (distance < nearestDistance)
		{
			nearestDistance = distance;
			nearest = i;
		}
	}
	return nearest;
}

glm::vec2 apple_barrel::BarrelSpot(glm::vec2 store, const glm::mat3& storeRotation, float storeRadius)
{
	// The model's own x axis, as the pit's turn carries it, along the ground
	const glm::vec2 facing {storeRotation[0][0], storeRotation[0][2]};
	const float length = glm::length(facing);
	if (length <= 0.0f)
	{
		return store;
	}
	return store + (facing / length) * storeRadius;
}

glm::vec3 apple_barrel::WormSpot(const glm::vec3& barrel, const glm::vec3& modelPoint)
{
	return barrel + modelPoint;
}

uint32_t apple_barrel::StepWorm(uint32_t place, uint32_t elapsed, uint32_t playTime, bool looping)
{
	if (playTime == 0)
	{
		return 0;
	}
	const auto next = static_cast<uint64_t>(place) + elapsed;
	if (looping)
	{
		return static_cast<uint32_t>(next % playTime);
	}
	return static_cast<uint32_t>(std::min<uint64_t>(next, playTime - 1));
}
