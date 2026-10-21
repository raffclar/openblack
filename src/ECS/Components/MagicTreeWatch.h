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

/// A villager that looks at the trees of a forest miracle: the tree it looks at now, the number of the next tree in the
/// forest's order, and how many trees it has looked at
struct MagicTreeWatch
{
	entt::entity tree {entt::null};
	uint32_t next {0};
	uint32_t looked {0};
};

} // namespace openblack::ecs::components
