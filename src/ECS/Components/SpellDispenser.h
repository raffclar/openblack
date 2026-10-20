/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"
#include "Magic/DispenserRules.h"

namespace openblack::ecs::components
{

/// A miracle dispenser: a building that floats a one-shot miracle above itself, and makes another some turns after it
/// is taken
struct SpellDispenser
{
	MagicType magicType {MagicType::None};
	magic::DispenserTimer timer;
	/// Its bubble while it has one, and where it made it
	entt::entity orb {entt::null};
	glm::vec3 orbPosition {0.0f};
	/// Its swirl on the land under it, 0 for none
	uint32_t effect {0};
	/// Its row in the buildings' table
	AbodeInfo building {AbodeInfo::None};
	/// The town it belongs to, none for none: a knock on it brings up that town's read-out. Its town mends it when it
	/// is damaged, and the player of its town can put a held miracle into it.
	entt::entity town {entt::null};
	/// Its town chose it for mending, and it is not whole yet: its town does not choose it again
	bool putUpForMending {false};
	/// How many of its town's passes over its buildings it has stood through, up to the age it counts as settled
	uint32_t age {0};
};

/// One of the dispensers the testbed lays out in a grid, cleared away when a scenario asks for none
struct TestbedDispenser
{
};

} // namespace openblack::ecs::components
