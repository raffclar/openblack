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

#include <string>

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::archetypes
{
class VillagerArchetype
{
public:
	/// Who a new villager joins: the town nearest its abode, or nobody yet (a newborn joins its mother's home)
	enum class Joins : uint8_t
	{
		NearestTown,
		Nobody,
	};
	static entt::entity Create(const glm::vec3& abodePosition, const glm::vec3& position, VillagerInfo type, uint32_t age,
	                           Joins joins = Joins::NearestTown);
	VillagerArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
