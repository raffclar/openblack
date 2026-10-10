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

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::archetypes
{
class VortexArchetype
{
public:
	/// A vortex of a kind with its middle at a point, in the state its kind starts in, that state beginning on a turn
	static entt::entity Create(const glm::vec3& centre, VortexType type, VortexStateType state, uint32_t turn);
	VortexArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
