/*******************************************************************************
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

namespace openblack::ecs::components
{

/// On the player's hand while a press of the right button on a creature waits for a tap. This occurs while the leash of
/// the player's creature is held loose in the hand. Only this computer keeps it. The world does not read it.
struct HandCreaturePress
{
	/// The creature that the button pressed. The hold goes to this creature, also when the hand moved off it.
	entt::entity creature {entt::null};
	/// When the button pressed, in milliseconds of the clock, and the game turn then
	uint32_t pressMs {0};
	uint32_t pressTurn {0};
};

} // namespace openblack::ecs::components
