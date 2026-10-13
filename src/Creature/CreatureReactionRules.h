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

#include <functional>
#include <optional>
#include <string_view>

#include <glm/vec2.hpp>

#include "Creature/CreatureDesires.h"
#include "Enums.h"

// What a creature does about the reactions spread round it: how urgent each kind is to it, and what it does once it
// takes one up. Most kinds it takes up and keeps for their time (which stops it taking up less urgent ones); several also
// make it drop what it was doing for a plan (look at the hand, examine a thing, sleep), teach it a skill, or move a
// desire. A few it heeds without ever keeping, so it carries on with what it reacted to before. Fire, teleport stones and
// miracles have rules of their own elsewhere. Pure rules, tested without the game.

namespace openblack::creature_reaction
{

/// Turns a creature waits after seeing a flying thing, or a thing crushed, before either can catch its eye again: it
/// heeds another only once more than this many have gone by
inline constexpr uint32_t k_SightingTurns = 899;
/// A creature heeds a flying thing or a crushed thing of another player's only when a draw below this comes under its
/// chance
inline constexpr uint32_t k_OtherPlayersDraw = 100;
/// Of that draw, these lowest values let it heed the thing
inline constexpr uint32_t k_OtherPlayersChance = 3;
/// The hand picking something up is this many times as urgent to a creature as the table says, kept to a byte
inline constexpr uint32_t k_HandPickUpUrgency = 3;
/// A creature sees what is within this many game-angle steps (of 2048 to the circle) of the way it looks, or in its own
/// map cell
inline constexpr uint32_t k_SightHalfAngle = 0x2aa;

/// Whether a creature sees a point: within its sight's half angle either side of the way it looks, or in the same map
/// cell as it. The angles are game angles; the way it looks is the way its head is turned to look at something, or the
/// way it faces when it looks at nothing.
[[nodiscard]] bool CanSee(uint16_t lookAngle, uint16_t angleToPoint, bool sameCell);
/// The game angle a creature looks along, from the way on the land it looks (x, z), as it keeps it: a quarter turn off
/// the heading that way, truncated to a game angle; none for no way at all
[[nodiscard]] uint16_t LookAngle(glm::vec2 way);

/// What a creature knows as it weighs a reaction
struct Facts
{
	/// The reaction table's urgency for the kind
	uint8_t tablePriority {0};
	/// The creature has a player, and a person plays it rather than the computer
	bool humanPlayer {false};
	/// The reaction belongs to a player, and not the creature's own
	bool otherPlayers {false};
	/// It sees the reaction's thing
	bool seesThing {false};
	/// The reaction's thing is the creature itself
	bool thingIsSelf {false};
	/// The reaction's thing is gone
	bool thingGone {false};
	/// The reaction's thing is a creature
	bool thingIsCreature {false};
	/// That creature is fighting this one
	bool thingFightsSelf {false};
	/// The game turn now, and when it last took up a flying thing or a crushed thing
	uint32_t turn {0};
	std::optional<uint32_t> sawFlying;
	std::optional<uint32_t> sawCrushed;
	/// What it is doing now: catching a thing, fighting, or anything done out of anger
	bool catching {false};
	bool fighting {false};
	bool angry {false};
	/// The flying thing was let go by a hand, and is still really in the air
	bool thrownFromHand {false};
	bool inTheAir {false};
	/// The hand picking something up is its own player's and holds something
	bool ownPlayersHand {false};
	bool handHolds {false};
};

/// How urgent a kind is to a creature before its distance counts, for the kinds a creature weighs its own way; none for
/// the kinds it weighs as everything alive does. A draw below a range is asked for only where the rule draws one.
[[nodiscard]] std::optional<uint8_t> Priority(Reaction type, const Facts& facts,
                                              const std::function<uint32_t(uint32_t)>& random);

/// A plan a reaction makes the creature drop what it was doing for: an action of the game's table, done for a desire, to
/// the reaction's thing or to the creature itself
struct Plan
{
	creature_desires::Desire desire {creature_desires::Desire::Curiosity};
	std::string_view action;
	bool onSelf {false};
};

/// What the creature does on taking up a reaction, in order: it comes across the thing, drops what it was doing for a
/// plan, learns a skill by watching, moves a source of its desires, and keeps the reaction or not
struct Setup
{
	/// It keeps the reaction for its time; when it doesn't, it carries on with what it reacted to before, if anything
	bool keeps {true};
	/// It notes the turn, for the flying thing's or crushed thing's wait
	bool notesFlying {false};
	bool notesCrushed {false};
	/// Its belief about the thing is brought up to date
	bool comesAcross {false};
	std::optional<Plan> plan;
	/// A skill of the game's table it learns by watching
	std::optional<size_t> skill;
	/// A source of its desires, by the game's numbering, moved by an amount
	std::optional<std::pair<uint32_t, float>> desireSource;
};

/// Ordinary skills of the game's table a creature learns by watching the hand: working a totem and filling a storage pit
inline constexpr size_t k_UseTotemSkill = 2;
inline constexpr size_t k_UseStoragePitSkill = 3;
/// Its sadness rises by this when it sees a creature die
inline constexpr float k_SadnessAtDeath = 1.0f;

/// What a creature does on taking up a kind of reaction; none for the kinds with rules of their own elsewhere (fire,
/// teleport stones, miracles, magic trees). A flying thing makes it look only while the thing is still in the air.
[[nodiscard]] std::optional<Setup> SetupFor(Reaction type, bool thingInTheAir);

} // namespace openblack::creature_reaction
