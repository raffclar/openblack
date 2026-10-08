/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TownCellObjects.h"

#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"

using namespace openblack::ecs;
using namespace openblack::ecs::systems;

std::vector<entt::entity> TownCellObjects::ObjectsInCell(glm::ivec2 cell) const
{
	return map_cells::ObjectsInCell(cell);
}

float TownCellObjects::Get2DRadius(entt::entity object) const
{
	return object::Get2DRadius(object);
}
