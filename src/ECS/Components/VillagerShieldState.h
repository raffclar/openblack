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

/// A villager amazed by a magic shield: the shield spell, the REACT_TO_MAGIC_SHIELD reaction it follows and the point
/// it looks at. Present only while it reacts to the shield.
struct VillagerShieldState
{
	entt::entity spell {entt::null}; ///< the shield spell it reacts to
	uint32_t reaction {0};           ///< the reaction it follows (0: none)
	glm::ivec2 lookAt {0, 0};        ///< the map point it looks at
};

} // namespace openblack::ecs::components
