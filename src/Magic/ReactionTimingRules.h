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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"
#include "Magic/VillagerReactionRules.h"

// How long a villager, creature or animal keeps reacting to a thing, and how long it waits before taking up a reaction of
// the same kind again. Both come from the reaction table's row, weighed by how far the one reacting is from the thing:
// the nearer it is, the longer it keeps reacting and the longer it waits. A creature weighs nearness its own way, by its
// own turns of the row. A few kinds have rules of their own on top: a creature, a predator, wood, a burning thing in the
// hand and a magic shield. Turns are whole game turns; a negative count means the reaction is over at once. Pure rules,
// tested without the game.

namespace openblack
{
struct ReactionInfo;
} // namespace openblack

namespace openblack::magic::reaction_timing
{

/// Who is reacting: a villager, a creature, or anything else alive (an animal)
enum class Reacting : uint8_t
{
	Villager,
	Creature,
	Other,
};

/// Which rule says how long the one reacting keeps at a kind of reaction, or waits before taking it up again
enum class Rule : uint8_t
{
	/// By the row and the distance
	Standard,
	/// A creature's reaction: by how impressive the creature is and how the town feels about it
	Creature,
	/// A predator: by whether it is an animal, how fast it runs and whether it is coming at the one reacting
	Predator,
	/// Wood (waiting only): a forester at work goes for wood again at once
	Wood,
	/// A burning thing in the hand: by how near it is and how fast it moves
	BurningObject,
	/// A magic shield: by how lately the villager's town was attacked
	Shield,
	/// Kinds no one ever holds, so none
	None,
};

/// The rule for how long a kind of reaction is kept
[[nodiscard]] Rule ReactRule(Reaction type);
/// The rule for how long the one reacting waits before taking a kind up again
[[nodiscard]] Rule AgainRule(Reaction type);

/// What a reaction table row says about time and distance
struct Row
{
	uint32_t turnsToReact {0};
	uint32_t turnsBeforeReactingAgain {0};
	uint32_t creatureTurnsToReact {0};
	uint32_t creatureTurnsBeforeReactingAgain {0};
	villager_reaction::Distance distance;
};
/// The time and distance a reaction table row gives
[[nodiscard]] Row RowOf(const ReactionInfo& info);

/// How many turns one keeps reacting at a distance from the thing. A villager or an animal keeps at it for the row's turns,
/// up to half as much again by its nearness within the row's distance plus ten metres (and less beyond that). A creature
/// keeps at it for the row's creature turns, by its nearness within the row's distance alone: the full turns at the
/// thing, cut by how much nearness counts at the edge of the row's distance.
[[nodiscard]] int32_t StandardTurnsToReact(const Row& row, Reacting who, float distance);
/// How many turns one waits before taking up a reaction of the kind again. A villager or an animal waits the row's turns,
/// longer the nearer it is, as above. A creature waits the row's creature turns, the nearer the shorter.
[[nodiscard]] int32_t StandardTurnsBeforeReactingAgain(const Row& row, Reacting who, float distance);

/// Turns that mean for ever
inline constexpr int32_t k_Forever = 0x7FFFFFFF;

/// What one reacting to a predator weighs
struct PredatorWatch
{
	/// Whether the predator is an animal
	bool animal {false};
	/// How fast the predator moves, in metres a second
	float speed {0.0f};
	/// Whether the predator is heading for the one reacting
	bool comingAt {false};
	/// How far apart they are, in metres
	float distance {0.0f};
	/// The predator row's least distance to run from it
	float leastRunDistance {0.0f};
	/// Whether the one reacting is looking at something (a villager's looking-at-a-thing state)
	bool lookingAtThing {false};
	/// For a villager: whether it is going to hide in a building or looking out to see whether it is safe yet
	bool hidingOrLooking {false};
	/// For a villager: whether it is looking out to see whether it is safe yet
	bool lookingIfSafe {false};
};
/// How many turns one keeps reacting to a predator. A villager hiding from it, or looking out to see whether it is safe,
/// keeps at it for ever. Otherwise a predator that isn't an animal ends it at once, and so does one running at more than
/// 5 metres a second straight at one that was looking at something; any other, the standard turns.
[[nodiscard]] int32_t PredatorTurnsToReact(const PredatorWatch& watch, Reacting who, int32_t standard);
/// How many turns one waits before reacting to a predator again: a villager looking out to see whether it is safe, not
/// at all; anyone else waits the standard turns only for an animal predator going no faster than 5 metres a second and
/// no nearer than the row's least distance to run, and not at all otherwise
[[nodiscard]] int32_t PredatorTurnsBeforeReactingAgain(const PredatorWatch& watch, Reacting who, int32_t standard);
/// Whether a thing moving along a heading over the land is coming at a point: the heading and the way from the thing to
/// the point, in three dimensions, are within about 37 degrees of each other
[[nodiscard]] bool ComingAt(const glm::vec3& thing, const glm::vec2& heading, const glm::vec3& point);

/// Whether a kind of reaction is to the one reacting itself rather than to the thing that made it (fainting, being
/// confused): its time is then weighed at no distance
[[nodiscard]] bool ReactsToItself(Reaction type);

} // namespace openblack::magic::reaction_timing
