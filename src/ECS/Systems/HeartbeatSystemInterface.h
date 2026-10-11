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

#include <optional>

#include "ECS/Heartbeat.h"
#include "Enums.h"

namespace openblack::ecs::systems
{

/// The players' heartbeats (components::Heartbeat): every turn each beats on, quickening with the danger to the
/// player's people, measured every ten turns, or held by the destruction of this computer's player's temple
class HeartbeatSystemInterface
{
public:
	virtual ~HeartbeatSystemInterface() = default;

	/// A game turn: the danger is measured when it is time, and every player's heart beats on
	virtual void ProcessTurn(uint32_t turn) = 0;
	/// A player's pulse a fraction of the way through the turn: 0 between beats, 1 at the top; 0 for a player without
	/// a heartbeat
	[[nodiscard]] virtual float Pulse(PlayerNames player, float turnFraction) const = 0;
	/// A player's heartbeat as it stands, none before their first turn
	[[nodiscard]] virtual std::optional<heartbeat::State> StateOf(PlayerNames player) const = 0;
};

} // namespace openblack::ecs::systems
