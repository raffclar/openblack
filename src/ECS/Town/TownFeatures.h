/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

// The town features of a map load: AssignTownFeatures, which only does the towns' forests (fields and fish farms take
// their town in their constructors).

namespace openblack::ecs::town_features
{
/// AssignForestsToTown's reference point: the storage pit's position when GetStoragePit gives one, else
/// GetTemporaryResourceStorePotOrPos(town position, WOOD) (which makes the town's MagicWood pot when it has none);
/// metres, altitude 0
[[nodiscard]] glm::vec3 ForestReference(entt::entity town);

/// MakeScenicForest for every town (newest first, town_queries::TownsNewestFirst) at its town rectangle's centre, then
/// AssignForestsToTown for every town in the same order (two separate passes). Called at the end of the map features'
/// load (after the map's script)
void AssignTownFeatures();
} // namespace openblack::ecs::town_features
