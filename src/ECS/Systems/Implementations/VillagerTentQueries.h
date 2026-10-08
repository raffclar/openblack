/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/VillagerTentQueriesInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The map's tree search and collision (ecs::map_cells)
class VillagerTentQueries final: public VillagerTentQueriesInterface
{
public:
	[[nodiscard]] entt::entity NearestTree(glm::ivec2 pos, float radius) const override;
	[[nodiscard]] uint32_t Collide(glm::ivec2 pos) const override;
};
} // namespace openblack::ecs::systems
