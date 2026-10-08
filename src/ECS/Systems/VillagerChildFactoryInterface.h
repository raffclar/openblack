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

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{
/// Makes the child a villager gives birth to
class VillagerChildFactoryInterface
{
public:
	virtual ~VillagerChildFactoryInterface() = default;

	/// A new villager of the info row and age at the position, in no town or abode yet
	[[nodiscard]] virtual entt::entity CreateChild(const glm::vec3& position, VillagerInfo info, uint32_t age) = 0;
};
} // namespace openblack::ecs::systems
