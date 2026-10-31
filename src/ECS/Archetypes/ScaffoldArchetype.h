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
class ScaffoldArchetype
{
public:
	/// A scaffold on the land at a place, for a town and a workshop (either can be none), turned by an angle about the up
	/// axis and drawn at a scale
	static entt::entity Create(const glm::vec3& position, entt::entity town, entt::entity workshop, float yAngleRadians,
	                           float scale);
	ScaffoldArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
