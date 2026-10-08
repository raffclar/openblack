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
class PotArchetype
{
public:
	/// allowEmpty: store piles exist with no resource (buried, not drawn) until the store fills them.
	static entt::entity Create(const glm::vec3& position, float yAngleRadians, PotInfo type, int32_t amount,
	                           bool allowEmpty = false);
	// Call after the amount changes: plain pots rescale, piles move to their new sink offset (over 1 s if animate).
	static void SetSize(entt::entity entity, bool animate);
	// Advances the pile sink animations (PileWood / PileFood::Draw).
	static void UpdateSizes(float seconds);
	PotArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
