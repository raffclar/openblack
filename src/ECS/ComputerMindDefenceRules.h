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
#include <bit>
#include <optional>
#include <span>

#include "Enums.h"
#include "PlayerSlots.h"

/// The rules by which a computer god finds how much it wants to defend its temple and its towns against the other
/// players. Pure functions of what the god sees.
namespace openblack::computer_mind_rules
{

/// The god does not react to a thing that hit its temple this number of game turns ago, or less
constexpr uint32_t k_RecentHitTurns = 20;
/// The share of the memory of a hit on the temple that the god loses each turn: it forgets the hit after 600 turns
constexpr float k_HitMemoryShare = std::bit_cast<float>(0x3ADA740Eu);

/// How much the god wants to react to the things that the other players throw at its temple, from the turn of the last
/// hit. None when nothing hit the temple, or when the hit is too recent. The value falls from 1 to 0 in 600 turns.
[[nodiscard]] std::optional<float> HitReaction(uint32_t turn, uint32_t lastHit);

/// The share of the memory of an attack on a town that the god loses each turn: it forgets the attack after 1200 turns
constexpr float k_AttackOnTownMemoryShare = std::bit_cast<float>(0x3A5A740Eu);

/// What the god knows of the last attack of one player on one of its towns
struct TownAttackFacts
{
	/// The index of the town in the list of the god's towns
	size_t town {0};
	PlayerNames attacker {PlayerNames::NEUTRAL};
	/// The turn of the last attack, and the aggression that the town keeps against the attacker
	uint32_t lastTurn {0};
	float aggression {0.0f};
};

/// How much the god wants to defend a town against one attacker: the aggression times two, no more than 1, times the
/// cube of the part of the attack that the town still remembers
[[nodiscard]] float DefendWish(uint32_t turn, uint32_t lastTurn, float aggression);

/// The attack that the god wants most to defend against, and how much
struct DefendChoice
{
	size_t index {0};
	float value {0.0f};
};
/// The attack that the god wants most to defend against, from the attacks in the order of the towns, then of the
/// players. An attack with no aggression does not count. The first attack that counts is the first choice, then only a
/// larger wish wins. None when no attack counts.
[[nodiscard]] std::optional<DefendChoice> ChooseDefence(std::span<const TownAttackFacts> attacks, uint32_t turn);

/// The number of players of a land whose belief gains a town looks at when it checks for an attack on its belief. The
/// seventh god and the neutral player do not count. The gods after the seventh count (deviation 167).
constexpr size_t k_BeliefAttackPlayers = 6;
/// A belief gain must be more than this to be an attack on the belief of a town
constexpr float k_LeastBeliefAttack = 0.008f;

/// What a town believes in the players, and what each player gave it lately, by player number. A place for each player
/// number of a network game: a player that the game does not have has 0.
struct TownBeliefFacts
{
	std::array<float, player_slots::k_PlayerNumbers> belief {};
	std::array<float, player_slots::k_PlayerNumbers> recent {};
};

/// How much a town of the god is under an attack on its belief: the belief in the player that gave the most belief
/// lately, against the belief in the god, doubled and squared, no more than 1. The god, and the players that the god
/// keeps as allies, do not count. `allies` has a value for each player number; a number past its end is no ally. 0 when
/// no gain is more than k_LeastBeliefAttack, or when the town does not believe in the god.
[[nodiscard]] float BeliefAttack(const TownBeliefFacts& town, PlayerNames god, std::span<const bool> allies);

/// The belief of a town in the god, less its largest belief in another player (0 when that is less than 0)
[[nodiscard]] float BeliefMargin(const TownBeliefFacts& town, PlayerNames god);

/// The town of the god that is most under an attack on its belief, and how much
struct BeliefAttackChoice
{
	size_t index {0};
	float value {0.0f};
};
/// The town that is most under an attack on its belief, from the towns in their order. A town counts when its attack is
/// more than 0 and its margin is not 0. The first of the largest wins. None when no town counts.
[[nodiscard]] std::optional<BeliefAttackChoice> ChooseBeliefDefence(std::span<const float> attacks,
                                                                    std::span<const float> margins);

/// The part of its belief in the god that a town gives to the player it believes in most of the other players: their
/// belief against the belief in the god. `rivals` are the beliefs in the other players that the god does not keep as
/// allies. 0 when the town does not believe in the god.
[[nodiscard]] float CloseRivalShare(float godBelief, std::span<const float> rivals);
/// How important the defence of the belief of a town of the god is: ten times the share of the close rival, no more than
/// 1. A town of another player always has 1.
constexpr float k_CloseRivalScale = 10.0f;
[[nodiscard]] float BeliefDefenceImportance(bool ownTown, float closeRivalShare);

/// How well a miracle suits the defence of the belief of a town: a miracle shield suits fully, a physical shield
/// suits at 0.9. A shield of the god that is already at the town does not suit. Other miracles do not suit.
constexpr float k_PhysicalShieldSuit = 0.9f;
[[nodiscard]] float BeliefDefenceSuit(MagicType type, bool ownShieldThere);

/// What the god knows of one of its towns when it looks at a creature of another player near it
struct TownNearCreature
{
	/// The distance from the creature to the centre of the town, in metres
	float distance {0.0f};
	/// The radius of the town, and the reach of its influence
	float radius {0.0f};
	float influence {0.0f};
};

/// A creature in this share of the radius of a town is inside the town
constexpr float k_InsideTownShare = 1.2f;

/// How much a creature of another player in the influence of the god is a threat: the nearer it is to the god's temple
/// against the god's influence power, the more. Each town adds 1 when the creature is inside it, then the nearness of
/// the creature against the reach of its influence.
[[nodiscard]] float CreatureThreat(float templeDistance, float influencePower, std::span<const TownNearCreature> towns);

/// The distance in metres that the god adds to two times the reach of its creature, when the creature helps at a place
constexpr float k_CreatureHelpReach = 200.0f;
/// The maximum distance in metres at which the creature of the god can help at a place, from the size of the creature
[[nodiscard]] float CreatureHelpReach(float size);

} // namespace openblack::computer_mind_rules
