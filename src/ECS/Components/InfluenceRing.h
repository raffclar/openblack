/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
/// A circle of a player's influence made by a script or a shield (a thing with a position, not an object). The logic
/// is in ECS/Influence (influence::CreateRing ...).
struct InfluenceRing
{
	glm::vec3 position; ///< follows the attached object (ProcessRings)
	PlayerNames player;
	float radius;
	bool anti;                          ///< an anti-influence ring: no influence of its player inside it
	entt::entity attached {entt::null}; ///< the object it follows (INFLUENCE_OBJECT), or none
};
} // namespace openblack::ecs::components
