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
class SpellSeedArchetype
{
public:
	/// The object part of a spell seed: the seed's mesh at a world point, at the scale `scale`
	/// (info.scale, x the icon's scale for icon seeds). The SpellSeed component is the caller's (Magic/Core/SpellSeed).
	static entt::entity Create(const glm::vec3& position, SpellSeedType seedType, float scale);
	SpellSeedArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
