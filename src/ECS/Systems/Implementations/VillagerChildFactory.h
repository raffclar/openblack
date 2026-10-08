/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/VillagerChildFactoryInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The game's villager archetype
class VillagerChildFactory final: public VillagerChildFactoryInterface
{
public:
	[[nodiscard]] entt::entity CreateChild(const glm::vec3& position, VillagerInfo info, uint32_t age) override;
};
} // namespace openblack::ecs::systems
