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

/// The apple barrel drawn beside the local player's storage pit: only drawn, never met by the cursor or anything else
struct AppleBarrel
{
	/// The storage pit it stands beside
	entt::entity store {entt::null};
	/// The worm on its top
	entt::entity worm {entt::null};
};

/// The worm wriggling on the apple barrel: only drawn, never met by the cursor or anything else
struct AppleBarrelWorm
{
	/// Its place in its clip, in milliseconds
	uint32_t place {0};
};

} // namespace openblack::ecs::components
