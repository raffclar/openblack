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

#include <algorithm>
#include <span>

/// Rules of the influence of the players that keep no state
namespace openblack::influence_rules
{

/// Whether a player can have influence at all: in a network game, only a player with a temple. In a game alone, every
/// player can, through their towns too.
[[nodiscard]] constexpr bool CanHaveInfluence(bool networkGame, bool hasTemple)
{
	return !networkGame || hasTemple;
}

/// A town of the player whose influence reaches the place: when the player gained it, and how far it reaches
struct ReachingTown
{
	uint32_t gained {0};
	float radius {0.0f};
};

/// Adds the reach of each town to the sum, in the order in which the player gained the towns, as the town list of the
/// player keeps them. Each town has its own gained number, so the order of the reaching towns is the order of the list.
/// The order of the sums is part of the answer, because each sum rounds.
[[nodiscard]] inline float AddReachingTowns(float sum, std::span<ReachingTown> towns)
{
	std::ranges::sort(towns, {}, &ReachingTown::gained);
	for (const auto& town : towns)
	{
		sum += town.radius;
	}
	return sum;
}

} // namespace openblack::influence_rules
