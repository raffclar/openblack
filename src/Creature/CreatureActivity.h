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

#include <optional>
#include <span>

#include "Creature/CreatureDesires.h"

/// The rules for the whole thing that a desire is about, and for the part of that thing that the creature acts on.
/// A desire about something (to impress, compassion, anger, the desires about other creatures) is about a whole thing:
/// a town, a forest, a temple, a flock or a creature. The creature acts on one part of the whole thing: one villager of
/// a town, one tree of a forest. A creature is its own part. Everything here is pure.
namespace openblack::creature_activity
{

/// The kinds of whole thing that a desire can be about
enum class Whole : uint8_t
{
	Town,
	Forest,
	Citadel,
	Creature,
	Flock,
};

/// The kind of whole thing that a belief kind is, none for a kind that is not a whole thing
[[nodiscard]] std::optional<Whole> WholeOf(uint32_t beliefKind);

/// What the creature knows about a whole thing when it examines a desire about it
struct WholeFacts
{
	Whole kind {Whole::Town};
	/// The whole thing has the player of the creature
	bool samePlayer {false};
	/// For a town: the belief of the town in the player of the creature is more than half of its largest belief in
	/// another player
	bool believesInPlayer {false};
	/// The player of the creature looks at the creature
	bool lookedAt {false};
	/// For a creature: the creature can attack it
	bool attackable {false};
};

/// Whether a desire can be about a whole thing.
/// - The desire to impress is about a town. The town of the creature's own player does not count when that town
///   believes in the player more than half as much as in any other player.
/// - Compassion is about a town or a creature.
/// - Anger is about a forest, a flock, a temple of another player, a creature that it can attack, or a town. The town
///   of its own player counts only while the player looks at the creature. When the creature must not attack its own
///   player, nothing of that player counts.
/// - The desires about other creatures are about a creature.
[[nodiscard]] bool CanBeAbout(creature_desires::Desire desire, const WholeFacts& whole, bool mayAttackOwnPlayer);

/// The kinds of part of a whole thing
enum class Part : uint8_t
{
	Villager,
	Animal,
	Tree,
	/// A building, a field or another object that does not live
	Building,
};

/// What the creature knows about a part when it examines a desire about the whole thing
struct PartFacts
{
	Part kind {Part::Villager};
	/// The data of the object lets a creature act on it. A thing that lives always lets a creature act on it.
	bool canActOn {true};
	/// The data of the object lets a creature attack it
	bool attackable {false};
	bool dead {false};
	bool dancing {false};
	/// A villager that a hand can pick up
	bool pickable {true};
};

/// Whether a creature can act on a part for a desire.
/// - To impress, it acts on a villager that does not dance.
/// - For compassion, it acts on a villager that a hand can pick up, or on any other part.
/// - For anger, it acts on a tree, on a thing that lives and is not dead, or on a building, if the data of the part lets
///   a creature attack it. All trees let a creature attack them.
[[nodiscard]] bool PartSuits(creature_desires::Desire desire, const PartFacts& part);

/// The first of the values that is the most and more than nothing, none when no value is more than nothing
[[nodiscard]] std::optional<size_t> MostUseful(std::span<const float> usefulness);

/// What the part that a plan acts on counts for in the priority of the plan. Anger counts the usefulness of the part,
/// but for a part of a town, which counts the default usefulness. Every other desire counts the default usefulness.
[[nodiscard]] float ActedOnUsefulness(creature_desires::Desire desire, Whole whole, float partUsefulness);

} // namespace openblack::creature_activity
