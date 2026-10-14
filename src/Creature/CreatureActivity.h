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

/// The kinds of thing that a creature can act on, by how the desires examine them
enum class Thing : uint8_t
{
	Villager,
	/// An animal or a bird
	Animal,
	Creature,
	/// A standing tree
	Tree,
	/// A rock, a bonfire or a dead tree
	Rock,
	/// A thing that lies about and moves: a pot, a pile, a ball, a creed or any other mobile object
	MobileObject,
	/// A static that is not a rock: the toys, the lanterns and the like
	MobileStatic,
	/// A thing that is set into the land: a building, the temple, a worship site, a feature or flowers
	Building,
	/// A field has the data of its town's field building
	Field,
	FishFarm,
	/// The totem statue of a town
	TotemStatue,
	/// A miracle
	Spell,
	/// Anything else, such as a whole thing
	Other,
};

/// What the creature knows about a thing when it examines a desire for it
struct ThingFacts
{
	Thing kind {Thing::Villager};
	/// The data of the thing lets a creature act on it. A thing that lives always lets a creature act on it.
	bool canActOn {true};
	/// The data of the thing lets a creature attack it, play with it or look it over
	bool attackable {false};
	bool playable {false};
	bool inspectable {false};
	bool dead {false};
	bool dancing {false};
	/// A villager that a hand can pick up
	bool pickable {true};
	/// The thing is the creature that examines the desire
	bool itself {false};
	/// A creature that a script made a friend of the creature
	bool friendOfCreature {false};
	/// An animal that frightens a creature: a lion, a tiger, a wolf, a leopard, a bat or a vulture
	bool frightening {false};
	/// A magic mushroom or a toadstool
	bool mushroom {false};
	/// How tall the thing is
	float height {0.0f};
	/// The creature can pick the thing up from where it is
	bool creatureCanPickUp {false};
};

/// A creature poos only on a thing that is taller than this
constexpr float k_TallestThingNotToPooOn = 1.5f;

/// Whether a creature can act on a thing for a desire. A thing must let a creature act on it. Then each desire has its
/// own rule:
/// - Impress: a villager that does not dance, or a totem statue.
/// - Compassion: a villager that a hand can pick up, or any other object.
/// - Anger: a tree or a rock. Any other object only when its data lets a creature attack it, it is not the creature
///   itself and it is not dead. A creature does not attack a creature that is its friend.
/// - Play: a thing that lies about, a static, a rock, a tree, a creature or a totem statue. A villager or an animal that
///   is not dead, or a building, only when its data lets a creature play with it.
/// - Fear: a creature, a miracle or an animal that frightens.
/// - Curiosity: an object whose data lets a creature look it over, and that is not dead.
/// - Poo: an object that does not live and is taller than k_TallestThingNotToPooOn.
/// - Tiredness and rest: a creature, or a thing that is set into the land, but not a field or a fish farm.
/// - Bring stuff home: a thing that the creature can pick up.
/// - The desires about other creatures: a creature.
/// - Get high: a magic mushroom.
/// - All other desires: any thing.
[[nodiscard]] bool ThingSuits(creature_desires::Desire desire, const ThingFacts& thing);

/// The first of the values that is the most and more than nothing, none when no value is more than nothing
[[nodiscard]] std::optional<size_t> MostUseful(std::span<const float> usefulness);

/// What the part that a plan acts on counts for in the priority of the plan. Anger counts the usefulness of the part.
/// Every other desire counts the default usefulness.
[[nodiscard]] float ActedOnUsefulness(creature_desires::Desire desire, float partUsefulness);

} // namespace openblack::creature_activity
