/*******************************************************************************
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
class FishFarmArchetype
{
public:
	/// A full fish farm in the middle of the map cell of a point, belonging to the town nearest it, with its shoal out in
	/// the open sea nearest it
	static entt::entity Create(const glm::vec3& position);
	FishFarmArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
