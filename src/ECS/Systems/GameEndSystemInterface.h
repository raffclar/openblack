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

#include <vector>

#include "ECS/GameEndRules.h"

namespace openblack::ecs::systems
{

/// The end of a skirmish or a network game (see game_end::ProcessTurn): each turn after the players' turns, the gods
/// with no temple are out, and the game is decided when one god is left
class GameEndSystemInterface
{
public:
	virtual ~GameEndSystemInterface() = default;

	/// The end of the game for one turn
	virtual void ProcessTurn(uint32_t turn) = 0;
	/// What happened in the last turn that the players see
	[[nodiscard]] virtual const std::vector<game_end::Event>& LastEvents() const = 0;
	/// The game is decided
	[[nodiscard]] virtual bool IsDecided() const = 0;
};

} // namespace openblack::ecs::systems
