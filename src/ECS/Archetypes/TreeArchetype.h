/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::archetypes
{
class TreeArchetype
{
public:
	static entt::entity Create(uint32_t forestId, const glm::vec3& position, TreeInfo type, bool isNonScenic,
	                           float yAngleRadians, float maxSize, float scale);
	/// A tree from a game's file. It is in no forest and draws no random numbers. As in the game after a load, it is
	/// not made to grow and it is part of the scenery.
	static entt::entity CreateLoaded(const glm::vec3& position, TreeInfo type, float yAngleRadians, float maxSize, float scale,
	                                 uint16_t growthCountdown);
	TreeArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
