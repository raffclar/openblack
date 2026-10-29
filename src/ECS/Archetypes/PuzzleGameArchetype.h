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

#include "ScriptHeaders/ScriptEnums.h"

namespace openblack::ecs::archetypes
{
class PuzzleGameArchetype
{
public:
	/// A puzzle game that a script makes, at a place, turned and scaled. It has no pieces and is never drawn.
	static entt::entity Create(const glm::vec3& position, float yAngleRadians, float scale, script::PuzzleGameType type);
	PuzzleGameArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
