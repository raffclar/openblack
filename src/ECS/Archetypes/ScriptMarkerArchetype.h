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

namespace openblack::ecs::archetypes
{
class ScriptMarkerArchetype
{
public:
	/// A point a script marks to come back to: it has a place and nothing else, and is never drawn
	static entt::entity Create(const glm::vec3& position);
	ScriptMarkerArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
