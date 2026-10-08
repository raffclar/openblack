/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ECS/Town/TownStores.h"
#include "Enums.h"

namespace openblack::ecs::systems
{
/// Where villagers leave resources outside a storage pit: the town's temporary store, and the log of dropped wood
class VillagerStoresInterface
{
public:
	virtual ~VillagerStoresInterface() = default;

	/// The town's temporary store of that type and its point (the store is made when the town has none)
	[[nodiscard]] virtual town_stores::TemporaryStore GetTemporaryStore(entt::entity town, const map_coords::MapCoords& from,
	                                                                    ResourceType type) = 0;
	/// Makes the dropped log and puts it into physics
	virtual void MakeDroppedLog(glm::vec3 position, uint32_t mesh, float multiplier, glm::vec3 velocity, glm::vec3 angular,
	                            std::optional<glm::vec3> momentum) = 0;
};
} // namespace openblack::ecs::systems
