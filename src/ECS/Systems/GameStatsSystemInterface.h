/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include "Game/GameStats.h"

namespace openblack::ecs::systems
{
/// Every player's GameStats and the shared statics, kept for the whole game; only game_stats::ClearAll clears them
/// (Locator::gameStatsSystem)
class GameStatsSystemInterface
{
public:
	virtual ~GameStatsSystemInterface() = default;

	/// One per player, PlayerNames as the index
	[[nodiscard]] virtual std::array<game_stats::Stats, game_stats::k_Players>& PlayerStats() = 0;
	/// The statics shared by all the players
	[[nodiscard]] virtual game_stats::Statics& SharedStats() = 0;
};
} // namespace openblack::ecs::systems
