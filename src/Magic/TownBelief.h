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
#include <optional>
#include <span>
#include <vector>

#include "Enums.h"

// A town's belief in each player. What its people are impressed by waits as pending belief until the town's next turn,
// when all of it, times the town's belief scale, is believed at once, no more than the town's cap for the player; a
// symbol of the belief gained then rises from the town centre in the player's colour. The town also keeps a tally of
// the belief each player has been given lately, which fades a little every turn. After its turn the town belongs to
// the player it believes in most: when that is another player, the town changes hands. Pure, tested on its own.

namespace openblack::magic::town_belief
{

inline constexpr size_t k_PlayerCount = static_cast<size_t>(PlayerNames::_COUNT);
/// Belief in a player is capped at this until a script sets another cap
inline constexpr float k_DefaultCap = 10.0f;

struct Belief
{
	/// What the town believes in each player
	std::array<float, k_PlayerCount> belief {};
	/// What it has been given and will believe at its next turn
	std::array<float, k_PlayerCount> pending {};
	/// What it has been given lately, fading every turn
	std::array<float, k_PlayerCount> recent {};
	/// The most it believes in each player
	std::array<float, k_PlayerCount> cap = [] {
		std::array<float, k_PlayerCount> caps {};
		caps.fill(k_DefaultCap);
		return caps;
	}();
	/// What pending belief is multiplied by as it is believed
	float scale {1.0f};
	/// What it believes in the neutral player, set again every turn: from the town tables, until a land script sets it
	float neutral {0.0f};
};

/// Belief in a player is set, no more than the town's cap for them
void Set(Belief& town, PlayerNames player, float value);

/// A land script sets the town's belief in a player; for the neutral player this is also what the town goes back to
/// believing in them every turn
void SetInPlayer(Belief& town, PlayerNames player, float value);

/// The town is given belief in a player
void Add(Belief& town, PlayerNames player, float amount);

/// Belief gained in a player at a turn
struct Gained
{
	PlayerNames player;
	float amount;
};

/// The town's turn: its recent tallies fade by the decay, and its pending belief is believed. The belief gained in each
/// player that gained any, in player order.
[[nodiscard]] std::vector<Gained> Turn(Belief& town, float recentDecay);

/// Who the town belongs to after its turn. Its belief in the neutral player is set back to what it keeps for them;
/// then the player it believes in most takes it, starting from its owner and going through every player in player
/// order, the neutral player last, a later player taking a tie. A new owner's belief is multiplied by the claimed-town
/// multiplier. None while its owner keeps it.
[[nodiscard]] std::optional<PlayerNames> Ownership(Belief& town, PlayerNames owner, float claimedMultiplier);

/// A player has lost a town: every town they still hold (the lost one among them) believes in them less, by the
/// lost-town multiplier and the land's lost-town scale
void LostTown(Belief& town, PlayerNames loser, float lostMultiplier, float lostTownScale);

/// A challenge script sets the town's belief in a player to a share of its belief in its owner
void SetRelativeToOwner(Belief& town, PlayerNames owner, PlayerNames player, float share);

/// A player takes a town by a script or a cheat. For the neutral player every player in the game loses all belief;
/// for any other, the player the town believes in most is halved and the taker believes that most plus the amount.
void TakeTown(Belief& town, PlayerNames player, float amount, std::span<const PlayerNames> playersInGame);

} // namespace openblack::magic::town_belief
