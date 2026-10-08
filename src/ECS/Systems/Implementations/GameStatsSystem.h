/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/GameStatsSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{
class GameStatsSystem final: public GameStatsSystemInterface
{
public:
	[[nodiscard]] std::array<game_stats::Stats, game_stats::k_Players>& PlayerStats() override { return _stats; }
	[[nodiscard]] game_stats::Statics& SharedStats() override { return _statics; }

private:
	std::array<game_stats::Stats, game_stats::k_Players> _stats {};
	game_stats::Statics _statics {};
};
} // namespace openblack::ecs::systems
