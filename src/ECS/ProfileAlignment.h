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

/// The player's alignment outlives their games: it is kept with their profile as they play, and a new game starts them
/// as good or evil as they were when it was last kept. A profile that has never kept one starts them at neither.
namespace openblack::profile_alignment
{

/// How many game turns pass between keeping the player's alignment with their profile: a minute at the usual speed
constexpr uint32_t k_TurnsBetweenKeeps = 600;

/// The alignment a new game starts the player at, from the one kept with their profile: no more good than 1, nor more
/// evil than -1
[[nodiscard]] constexpr float Starting(double kept)
{
	return static_cast<float>(std::clamp(kept, -1.0, 1.0));
}

/// Whether the player's alignment is due to be kept with their profile at this game turn, it having last been kept at
/// another. The turns count again from nothing on each new land while the turn it was last kept at stays, so the first
/// frame of a new land always keeps it.
[[nodiscard]] constexpr bool DueToKeep(uint32_t turn, uint32_t lastKept)
{
	// Unsigned: a turn before the one last kept wraps round to a great many turns since
	return turn - lastKept >= k_TurnsBetweenKeeps;
}

} // namespace openblack::profile_alignment
