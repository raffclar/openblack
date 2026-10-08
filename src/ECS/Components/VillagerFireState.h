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

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

namespace openblack::ecs::components
{

/// A villager's fire fields: the fire it flees or fights, the REACT_TO_FIRE reaction it follows and the object of
/// it, and the destination it had before the fire. Added the first time the villager's fire code needs it and kept
/// until the map is cleared.
struct VillagerFireState
{
	uint32_t fire {0};                 ///< the FireEffect id it flees or fights (0: none)
	entt::entity object {entt::null};  ///< the object of the reaction it follows
	uint32_t reaction {0};             ///< the reaction it follows (0: none)
	glm::vec2 savedDestination {0.0f}; ///< where it was going, restored when it stops being on fire
};

} // namespace openblack::ecs::components
