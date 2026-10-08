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
class OneOffSpellSeedArchetype
{
public:
	/// The mesh of the one-shot orbs (the shared mesh .\data\spells\meshes\O_Bibble_up.l3d; loaded with the loose meshes
	/// in Game.cpp)
	static constexpr const char* k_MeshName = "O_Bibble_up";

	/// A mobile object of the one-off spell seed info at the position, with the shared mesh. seedType must be 0..29;
	/// entt::null otherwise.
	static entt::entity Create(const glm::vec3& position, SpellSeedType seedType, int powerUp, float scale);
	OneOffSpellSeedArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
