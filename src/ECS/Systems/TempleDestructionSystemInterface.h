/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{

/// A temple whose heart has lost all its life is destroyed over 22 seconds: its sounds, its glow, its explosion and smoke,
/// then it goes. The local player's losing their temple ends the game.
class TempleDestructionSystemInterface
{
public:
	virtual ~TempleDestructionSystemInterface() = default;

	/// The temple's heart has lost the last of its life: its destruction starts, or starts again from
	/// nothing
	virtual void Start(entt::entity temple) = 0;
	/// At the start of each game turn: the sound loops of temples that went some other way stop
	virtual void StartTurn() = 0;
	/// At the start of a player's turn: the player's temple, if it is being destroyed, moves on a turn, and then its
	/// heart beams if it is hurt
	virtual void ProcessPlayerTurn(PlayerNames player) = 0;
	/// The start of the turn, then each player's temple in the order of the players
	virtual void ProcessTurn() = 0;
	/// At the end of each game turn, after the physics: the game ends for the local player whose temple is being
	/// destroyed, in the turn its heart lost the last of its life
	virtual void EndTurn() = 0;
};

} // namespace openblack::ecs::systems
