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

/// On the player's hand after the hand sent the end of the lock on a creature (player_commands::HandUnlock), until the
/// world ends the lock. The hand does not send the end again. Only this computer keeps it. The world does not read it.
struct HandLockEndSent
{
	/// The world save and the world parts that the server sends do not keep it. After a load, the hand sends the end again.
	static constexpr bool k_LocalOnly = true;

	/// The creature whose lock the hand sent the end of
	entt::entity creature {entt::null};
	/// An end of the interface lock sent it and stopped the hold. The hand is free, and the next ends of the interface lock
	/// do not send it again. The hand keeps it until the turn ends the lock.
	bool atLockEnd {false};
};

} // namespace openblack::ecs::components
