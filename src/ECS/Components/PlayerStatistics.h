/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>

#include "ECS/StatisticsRules.h"

namespace openblack::ecs::components
{

/// What the game counts of a player for its statistics: the temple's world scroll and the Game Statistics box. The
/// counters start again on each land; the histories of influence and population run on over the whole game.
struct PlayerStatistics
{
	/// The game's magic types, None first
	static constexpr size_t k_MagicTypeCount = 42;

	/// Children born to the player's people
	uint32_t births {0};
	/// The player's people who died, and the deaths put down to the player
	uint32_t deaths {0};
	uint32_t peopleKilled {0};
	/// Villagers and animals sacrificed for the player
	uint32_t sacrifices {0};
	/// The people of the towns the player took over, as they were taken
	uint32_t peopleConverted {0};
	/// The player's buildings finished, by kind
	statistics::BuildingsBuilt built;
	/// Buildings of others the player put out of action
	uint32_t buildingsDestroyed {0};
	/// Disciples made of each kind the game counts
	statistics::DisciplesMade disciplesMade {};
	/// Food the player's people ate and wood their towns built with
	uint32_t foodUsed {0};
	uint32_t woodUsed {0};
	/// Gods the player saw defeated
	uint32_t godsDefeated {0};
	/// How many of each magic type the player has cast
	std::array<uint32_t, k_MagicTypeCount> castsOfType {};
	/// All the prayer power the player's worship sites have given
	float chantsUsed {0.0f};
	/// How satisfied the player's people have been
	statistics::Satisfaction satisfaction;
	/// The most and fewest people, men and women in the player's towns, checked every turn
	statistics::Extremes<uint32_t> population;
	statistics::Extremes<uint32_t> men;
	statistics::Extremes<uint32_t> women;
	/// The most and least belief in the player summed over every town
	statistics::Extremes<float> belief;
	/// The player's alignment and their creature's as the land started, and their creature's size the first time it
	/// was seen on the land (0 until then)
	float startAlignment {0.0f};
	float startCreatureAlignment {0.0f};
	float startCreatureSize {0.0f};

	/// The player's influence and population over the whole game
	statistics::History influenceHistory;
	statistics::History populationHistory;
};

} // namespace openblack::ecs::components
