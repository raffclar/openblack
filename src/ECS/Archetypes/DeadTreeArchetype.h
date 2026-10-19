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

#include "3D/AllMeshes.h"
#include "Enums.h"

namespace openblack::ecs::archetypes
{
class DeadTreeArchetype
{
public:
	/// The ground a dead tree covers as an obstacle follows how it lies, once it has come to rest
	static void FitObstacle(entt::entity deadTree);
	static entt::entity Create(const glm::vec3& position, TreeInfo type, float yAngleRadians, float scale);
	/// A dead tree that a land script makes: it turns about the vertical, then leans about x and z, as a mobile static does
	static entt::entity Create(const glm::vec3& position, TreeInfo type, float xAngleRadians, float yAngleRadians,
	                           float zAngleRadians, float scale);
	/// How a dead tree from a land script lies: turned about y, then about x, then about z, with the game's maths
	[[nodiscard]] static glm::mat3 LandScriptRotation(float xAngleRadians, float yAngleRadians, float zAngleRadians);
	/// A dead tree of a kind drawn with a model of its own, turned as given: the log a villager lets fall
	static entt::entity Create(const glm::vec3& position, TreeInfo type, const glm::mat3& rotation, float scale, MeshId model);
	DeadTreeArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
