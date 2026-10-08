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
class AnimalArchetype
{
public:
	/// CREATE_ANIMAL / CREATE_NEW_ANIMAL: with a flock (a components::Flock entity) the animal joins it (age 0 means
	/// GameRand(20) + 5); without one, as the overload below. Returns entt::null for the species the original's class
	/// switch doesn't make and for the flying animals, not created yet.
	static entt::entity Create(const glm::vec3& position, AnimalInfo type, entt::entity town, entt::entity flock, uint32_t age);
	/// Without a town (CHL CREATE): the animal gets a flock of its own; age 0 means GameRand(40) + 5. The int is unused.
	static entt::entity Create(const glm::vec3& position, AnimalInfo type, int32_t, uint32_t age);
	AnimalArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
