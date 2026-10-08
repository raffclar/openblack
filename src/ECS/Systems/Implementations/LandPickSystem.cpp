/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "LandPickSystem.h"

#include "3D/LandIslandInterface.h"
#include "3D/LandUnderPixel.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

std::optional<glm::vec3> LandPickSystem::LandUnderPixel(glm::vec3 camera, glm::vec3 nearPoint, bool withSea) const
{
	if (!Locator::terrainSystem::has_value())
	{
		return std::nullopt;
	}
	return land_pick::UnderPixel(Locator::terrainSystem::value(), camera, nearPoint, withSea);
}

std::optional<glm::vec2> LandPickSystem::LandAlong(glm::vec3 from, glm::vec3 to) const
{
	glm::vec2 hit(0.0f);
	if (!Locator::terrainSystem::has_value() || !Locator::terrainSystem::value().RayCastLand(from, to, hit))
	{
		return std::nullopt;
	}
	return hit;
}
