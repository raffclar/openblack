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
#include <functional>
#include <optional>

#include "Enums.h"

namespace openblack::ecs
{
class Registry;
} // namespace openblack::ecs

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

/// Whether a new game starts the player at the alignment of the profile of this computer. Only a game for one player
/// does. In a network game each god starts at the alignment that its player sent with the start data, so that each
/// computer starts the same world. A game server has no player and no start data of its own.
[[nodiscard]] constexpr bool StartsFromOwnProfile(bool networkGame)
{
	return !networkGame;
}

/// The alignment that this computer sends with its start of a network game, from the one kept with its profile: no more
/// good than 1, nor more evil than -1. A kept value that is not a number sends -1.
[[nodiscard]] constexpr float Sent(double kept)
{
	if (kept > 1.0)
	{
		return 1.0f;
	}
	if (!(kept >= -1.0))
	{
		return -1.0f;
	}
	// The kept value goes to the nearest float
	return static_cast<float>(kept);
}

/// The alignment that a god of a network game starts at, from the alignment that its player sent with the start: no more
/// good than 1, nor more evil than -1. A god with no player that sent a start, as a slot that the computer fills,
/// starts at neither good nor evil. A value that is not a number starts the god at -1.
[[nodiscard]] constexpr float StartsAt(std::optional<float> sent)
{
	const float value = sent.value_or(0.0f);
	if (!(value >= -1.0f))
	{
		return -1.0f;
	}
	return value > 1.0f ? 1.0f : value;
}

/// The alignment that the player of a god sent with the start of a network game. None when no player sent one.
using SentAlignment = std::function<std::optional<float>(PlayerNames god)>;

/// At the start of a network game, each player of the world starts at the alignment that the player of its god sent
/// with the start. A god with no such player starts at 0. The neutral player does not change.
void StartGods(ecs::Registry& registry, const SentAlignment& sent);

/// Whether the game keeps the player's alignment with the profile of this computer. A network game does not change
/// the profile's alignment.
[[nodiscard]] constexpr bool KeptInOwnProfile(bool networkGame)
{
	return !networkGame;
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
