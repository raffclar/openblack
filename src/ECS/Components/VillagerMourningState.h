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

namespace openblack::ecs::components
{

/// A villager mourning a death: the dead villager and the REACT_TO_DEATH reaction it follows. Removed when it stops
/// reacting.
struct VillagerMourningState
{
	entt::entity dead {entt::null}; ///< the dead villager it mourns
	uint32_t reaction {0};          ///< the reaction it follows (0: none)
};

} // namespace openblack::ecs::components
