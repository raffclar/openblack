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

#include "Enums.h"

namespace openblack::ecs::components
{

/// On a villager hiding from something in a building of its town: the building, its turns inside or looking about
/// since, and what frightened it while that is known (with the kind of reaction whose keep-away distance applies)
struct VillagerHiding
{
	entt::entity building {entt::null};
	uint32_t turns {0};
	entt::entity threat {entt::null};
	Reaction threatKind {Reaction::None};
};

/// On a villager panicking in the open: the point it ran from, which it turns to look back at
struct VillagerPanic
{
	glm::vec2 point {0.0f};
};

} // namespace openblack::ecs::components
