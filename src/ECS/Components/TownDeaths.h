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

#include <array>

#include "Enums.h"

namespace openblack::ecs::components
{

/// The town's death counters: town stats fields that only VillagerDead writes and the town stats' constructor
/// zeroes; the stats' per-turn recount (ECS/Town/TownStats.cpp) does not touch them, so they live in their own
/// component (ecs::villager::VillagerDead assigns it, zeroed, at a town's first death: the same as zero at the town's
/// creation). Readers found: the deaths by reason (GET_TOWN_WORSHIP_DEATHS reads byReason[4]); readers relative to
/// the town stats were not searched.
struct TownDeaths
{
	uint32_t total38 {0};                                                       ///< a total, ++ on every death
	std::array<uint32_t, 8> total5C {};                                         ///< only [0] is written, ++ on every death
	std::array<uint32_t, static_cast<size_t>(DeathReason::_COUNT)> byReason {}; ///< by DeathReason, ++
	std::array<uint32_t, 9> byPlayer {}; ///< by the killer's player number, ++ (none: the neutral)
	uint32_t lastDeathTurn {0};          ///< the game turn of the last death
	uint32_t count {0};                  ///< ++ on every death
};

} // namespace openblack::ecs::components
