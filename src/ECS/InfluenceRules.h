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

/// One building of a town, as its influence uses it: the part of it that stands built, its scale, its life, the
/// influence of its row in the buildings' table, and how many people live in it
struct BuildingInfluence
{
	float built {1.0f};
	float scale {1.0f};
	float life {1.0f};
	float influence {0.0f};
	uint32_t people {0};
};

/// The influence that one building adds to its town. Its size is its scale times its built part, then times its life,
/// then times the influence of its row. The building adds this size once for itself and once for each person in it.
/// Each step rounds, thus the order of the steps is part of the answer.
[[nodiscard]] inline float OfBuilding(const BuildingInfluence& building)
{
	const float share = building.scale * building.built;
	const float size = (building.life * share) * building.influence;
	return (static_cast<float>(building.people) + 1.0f) * size;
}

/// The town adds the influence of each building to the sum that it has, one building at a time
[[nodiscard]] constexpr float AddBuilding(float sum, float building)
{
	return building + sum;
}

/// The town's influence at the end of the pass over its buildings: the land's multiplier times the sum
[[nodiscard]] constexpr float TimesMultiplier(float sum, float multiplier)
{
	return multiplier * sum;
}

} // namespace openblack::influence_rules
