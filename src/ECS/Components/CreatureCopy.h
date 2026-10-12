/******************************************************************************
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

#include "Creature/CaveCreature.h"

namespace openblack::ecs::components
{

/// A copy of a creature's body drawn somewhere else, posed on its own: it looks as the creature does (its shape, skin
/// and tattoos are read from the creature each frame), while its own CreatureAnimation, CreatureEyes and Transform say
/// how it is posed and where it stands. It is not a creature: nothing that looks for creatures finds it.
struct CreatureCopy
{
	/// The creature it is a copy of
	entt::entity creature {entt::null};
	/// The size it is drawn at
	float size {1.0f};
};

/// The copy of the player's creature that the Creature Cave shows by its fire
struct CaveCreature
{
	cave_creature::State state;
	/// Where the creature is lit from: the cave's fire
	glm::vec3 light {0.0f};
};

} // namespace openblack::ecs::components
