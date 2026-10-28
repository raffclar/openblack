/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/HandSystemInterface.h"
#include "Enums.h"

namespace openblack::input
{

/// The player that the input of this computer acts for: the god of the hands that this computer moves. In a single
/// player game, this is the first player. In a network game, this is the god of the slot of this computer. The mouse,
/// the keys and the gestures use this player. The turn uses the sender of each command.
[[nodiscard]] inline PlayerNames LocalPlayer(const ecs::systems::HandSystemInterface* hands) noexcept
{
	return hands != nullptr ? hands->GetOwnPlayer() : PlayerNames::PLAYER_ONE;
}

/// Whether the player is the god of this computer. The help, the sounds and the files of this computer are for this
/// player only. With no hands, only the first player is the god of this computer. A computer with no player of its own,
/// for example the game server, has no such god.
[[nodiscard]] inline bool IsLocalPlayer(const ecs::systems::HandSystemInterface* hands, PlayerNames player) noexcept
{
	return hands != nullptr ? hands->IsOwnPlayer(player) : player == PlayerNames::PLAYER_ONE;
}

} // namespace openblack::input
