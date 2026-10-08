/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/VillagerStoresInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The towns' temporary stores (ecs::town_stores) and the dropped logs of the tree and physics code
class VillagerStores final: public VillagerStoresInterface
{
public:
	town_stores::TemporaryStore GetTemporaryStore(entt::entity town, const map_coords::MapCoords& from,
	                                              ResourceType type) override;
	void MakeDroppedLog(glm::vec3 position, uint32_t mesh, float multiplier, glm::vec3 velocity, glm::vec3 angular,
	                    std::optional<glm::vec3> momentum) override;
};
} // namespace openblack::ecs::systems
