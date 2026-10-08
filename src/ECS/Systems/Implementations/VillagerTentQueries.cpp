/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerTentQueries.h"

#include "3D/MapCoords.h"
#include "ECS/MapCells.h"
#include "ECS/Villager/VillagerHome.h"

using namespace openblack::ecs;
using namespace openblack::ecs::systems;
namespace map_coords = openblack::map_coords;

entt::entity VillagerTentQueries::NearestTree(glm::ivec2 pos, float radius) const
{
	return villager::FindNearestTree(pos, radius);
}

uint32_t VillagerTentQueries::Collide(glm::ivec2 pos) const
{
	return map_cells::Collide(map_coords::MapCoords {pos.x, pos.y, 0.0f});
}
