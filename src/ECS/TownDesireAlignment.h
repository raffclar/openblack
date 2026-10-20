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
#include <span>

#include "ECS/Components/TownDesire.h"

/// How the wants of a town move its player's alignment. Each turn of the town, a want that stays over its level counts
/// one more turn, and a want under its level counts one turn less. The longer a want stays over its level, the more it
/// moves the alignment, by the cube of its share of the most turns. The change grows with the share of all people that
/// live in the town, and with how much the town wants the thing. Pure rules on the town's values, tested without the
/// game.
namespace openblack::ecs::town_desire_alignment
{

/// What the table of desires gives one desire for the alignment
struct DesireRule
{
	/// The level over which the want counts
	float affectsAfter {0.0f};
	/// How much the want moves the alignment
	float importance {0.0f};
	/// The turns over the level after which the want has its full effect
	uint32_t maxTurns {0};
};

/// What the town wants of each thing, with its two boosts
struct Wants
{
	std::span<const float, components::k_TownDesireCount> boostA;
	std::span<const float, components::k_TownDesireCount> desire;
	std::span<const float, components::k_TownDesireCount> boost;
};

/// The people that the alignment change counts: those of the town, and those of all the players
struct People
{
	uint32_t town {0};
	uint32_t all {0};
};

/// A town's turn: the turns over the level of each want go up or down, and the change of the alignment comes back. The
/// sum of the changes of the wants is held from -1 to 1, then multiplied by the most change a turn.
[[nodiscard]] float Change(std::array<uint32_t, components::k_TownDesireCount>& turnsOver, const Wants& wants,
                           std::span<const DesireRule, components::k_TownDesireCount> rules, People people,
                           float maxChangePerTurn);

} // namespace openblack::ecs::town_desire_alignment
