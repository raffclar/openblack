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

#include <unordered_map>

#include <entt/entity/entity.hpp>

namespace openblack::ecs::systems
{
/// The villagers' shared state: the villager whose physics is being ended, and how many Livings took each mourning
/// reaction (villager death and villager_mourning go through it)
class VillagerStateSystemInterface
{
public:
	virtual ~VillagerStateSystemInterface() = default;

	/// entt::null outside an EndingPhysicsScope; a nested scope saves and puts back the outer one
	[[nodiscard]] virtual entt::entity& EndingPhysics() = 0;
	/// By reaction id; only looked up, never walked
	[[nodiscard]] virtual std::unordered_map<uint32_t, uint32_t>& MourningTakers() = 0;
};
} // namespace openblack::ecs::systems
