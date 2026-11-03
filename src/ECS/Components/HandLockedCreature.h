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

namespace openblack::ecs::components
{

/// On the player's hand while the world has it hold a creature: from the turn that does the lock of the press on the
/// creature (player_commands::HandLock) to the turn that does its end. The world reads the held creature only from here,
/// never from the hand that the frames hold to the creature.
struct HandLockedCreature
{
	entt::entity creature {entt::null};
	/// The button that holds the creature let go (player_commands::HandLockRelease). The creature does not look at the
	/// hand. The lock stays until the hand sends its end, when the creature is still.
	bool released {false};
};

} // namespace openblack::ecs::components
