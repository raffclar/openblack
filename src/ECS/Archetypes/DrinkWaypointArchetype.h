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
class DrinkWaypointArchetype
{
public:
	/// A water point that the land script marks: a place to drink, which is never drawn
	static entt::entity Create(const glm::vec3& position);
	DrinkWaypointArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
