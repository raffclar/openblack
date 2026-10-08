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
#include <glm/vec2.hpp>

namespace openblack::ecs::systems
{
/// The map queries a homeless villager makes to pitch a tent: the nearest tree and the collision at a cell
class VillagerTentQueriesInterface
{
public:
	virtual ~VillagerTentQueriesInterface() = default;

	/// The nearest tree within radius of the x / z map position, or null
	[[nodiscard]] virtual entt::entity NearestTree(glm::ivec2 pos, float radius) const = 0;
	/// The collision flags at the x / z map position
	[[nodiscard]] virtual uint32_t Collide(glm::ivec2 pos) const = 0;
};
} // namespace openblack::ecs::systems
