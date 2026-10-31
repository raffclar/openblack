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
#include "PlayerSlots.h"

// How a town remembers being attacked. Each harm done to it adds to the aggression it holds against whoever did it,
// the first harm by a player more; the more often it is attacked the less each attack adds. Each turn the aggression
// fades a little: what its own player did is its want of mercy, what anyone else did its want of protection, which
// its desires take up. The last attacker and the turn it attacked are kept, which the villagers sheltering under a
// shield look at. The town also keeps how much each player's last attack burnt, crushed and hit, which the computer gods
// look at to choose a shield. Pure rules on the town's record, tested without the game.

namespace openblack::ecs::town_aggression
{

/// How much an attack burnt, crushed and hit what it struck: the values of its effect before any defence
struct Harm
{
	float burn {0.0f};
	float crush {0.0f};
	float hit {0.0f};
};

/// What a town keeps of the attacks on it
struct Record
{
	/// The aggression held against each player, and the turn each last attacked
	std::array<float, player_slots::k_PlayerNumbers> aggression {};
	std::array<uint32_t, player_slots::k_PlayerNumbers> lastTurns {};
	/// The last attacker, and the turn it attacked; none before any attack
	PlayerNames lastAggressor {PlayerNames::NEUTRAL};
	uint32_t lastTurn {0};
	/// How much of an attack counts, by someone else and by the town's own player, worn down by each
	float protectionMultiplier {1.0f};
	float mercyMultiplier {1.0f};
	/// This turn's wants, the sums of the aggression
	float protection {0.0f};
	float mercy {0.0f};
	/// The harm of the last attack of each player. Last in the record so that a save of an older build still reads the
	/// fields before it.
	std::array<Harm, player_slots::k_PlayerNumbers> lastHarms {};
};

/// The town is attacked by a player with an amount of harm: the first harm a player does adds a bonus; it counts by
/// the multiplier for the town's own player or for anyone else, which is then worn down by a tenth. The town keeps the
/// harm of the attack as the last harm of the player.
void Attacked(Record& record, PlayerNames aggressor, bool aggressorIsOwner, float amount, float firstTimeAddition,
              uint32_t turn, const Harm& harm);
/// The same for an attack that did no harm
void Attacked(Record& record, PlayerNames aggressor, bool aggressorIsOwner, float amount, float firstTimeAddition,
              uint32_t turn);

/// A turn: the aggression fades, and is summed into the wants of mercy (the owner's) and protection (anyone else's, or
/// everyone's in a neutral town); a multiplier recovers while its want is small
void ProcessTurn(Record& record, PlayerNames owner);

/// Whether harm done to a town's thing counts as an attack on the town: any harm does, even from a thing whose kind adds
/// no aggression, so the attacker, the turn, the first-attack bonus and the worn multiplier are still recorded
[[nodiscard]] constexpr bool CountsAsAttack(float damage)
{
	return damage != 0.0f;
}

/// The harm an attack does a town's thing, as aggression: the harm times the thing's kind's aggressor value
[[nodiscard]] constexpr float AggressionFromDamage(float damage, float aggressorValue)
{
	return damage * aggressorValue;
}

/// The seconds since a player last attacked the town, for a script. The answer is the largest float when the player
/// number is not valid or the town holds 0.15 or less against the player.
[[nodiscard]] float SecondsSinceAttacked(const Record& record, PlayerNames player, uint32_t turn, uint32_t millisecondsPerTurn);

} // namespace openblack::ecs::town_aggression
