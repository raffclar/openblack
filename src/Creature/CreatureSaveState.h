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

#include <bitset>
#include <functional>
#include <optional>

#include <CreatureRecord.h>
#include <SavedObjects.h>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureFizz.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureSpells.h"
#include "Creature/LeashRules.h"
#include "Enums.h"

// The rules that turn the creature's state into the creature part of a saved game and back, as the game does it.
//
// - The game keeps the plan and the agenda of a creature only while a script controls it, while it fights, or while
//   villagers dance with it. Any other creature chooses again after a load.
// - The game saves the fizz of a creature only while the invisibility spell is on it. Else it writes no fizz, and the
//   creature that it saved is in full sight again at once.
// - A load gives the creature the fresh mind of its mind file, then the values of the record.
// - When the mind file of a creature is missing or empty at a load, the game does not put the creature back on the map.
//   The creature is lost from the land.
namespace openblack::creature_save_state
{

/// The position flags that the game's saved games have for a player's creature on the land: on the map, and two flags
/// that every creature of the game has
inline constexpr uint16_t k_CreaturePositionFlags = 0x0103;
/// The speed word that every creature of the game's saved games has
inline constexpr uint16_t k_CreatureSpeedWord = 0x05C3;

/// The reference that the saved game keeps for an entity: no object for none
[[nodiscard]] gamesave::ObjectRef ToRef(entt::entity entity);
[[nodiscard]] gamesave::ObjectRef ToRef(std::optional<entt::entity> entity);
/// The entity of a reference, or nothing for no object or an object of a file of the game that has no entity
[[nodiscard]] std::optional<entt::entity> FromRef(const gamesave::ObjectRef& ref);

/// Whether the save keeps the plan and its agenda
[[nodiscard]] bool KeepsPlan(bool scriptControlled, bool fighting, size_t dancers);

/// The slot as the save keeps it. The slot's place gives its spell.
[[nodiscard]] gamesave::SpellSlot ToSlot(const creature_spells::Slot& slot, uint32_t place);
[[nodiscard]] creature_spells::Slot FromSlot(const gamesave::SpellSlot& slot);
[[nodiscard]] gamesave::QueuedSpell ToQueued(const creature_spells::Waiting& waiting);
/// A queued spell gets its turns from its seconds, as when the game starts it
[[nodiscard]] std::optional<creature_spells::Waiting> FromQueued(const gamesave::QueuedSpell& queued, float turnsPerSecond);

/// The fizz that the save writes: the creature's fizz while the invisibility spell is on it, else none
[[nodiscard]] creature_fizz::Fizz FizzToSave(const creature_fizz::Fizz& fizz, const creature_spells::Spells& spells);

/// The leash types that a creature knows, as the save keeps them
[[nodiscard]] gamesave::KnownLeashes ToKnown(const std::bitset<creature_leash::k_Types.size()>& known);
[[nodiscard]] std::bitset<creature_leash::k_Types.size()> FromKnown(const gamesave::KnownLeashes& known);

/// The number that the save keeps for a player. A creature that belongs to no one (openblack's neutral player) has
/// k_None, as the game's creatures with no player have.
[[nodiscard]] uint8_t PlayerNumber(std::optional<PlayerNames> player);
/// The player of a number of the save. k_None and the game's last player are openblack's neutral player.
[[nodiscard]] std::optional<PlayerNames> PlayerFromNumber(uint8_t number);

/// The leash type as the save keeps it, and back: 1 evil, 2 the learning leash, 3 good
[[nodiscard]] uint32_t LeashTypeNumber(LeashType type);
[[nodiscard]] std::optional<LeashType> LeashTypeFromNumber(uint32_t number);

/// Whether a load puts the creature back on the map, from the size of its mind file (none when the file does not open).
/// Only a mind file that opens and is not empty does this.
[[nodiscard]] bool ResolvesOnLoad(std::optional<size_t> mindFileSize);

/// The agenda that a load gives back: the game starts the step that the creature did again from its beginning
[[nodiscard]] gamesave::Agenda AgendaAfterLoad(gamesave::Agenda agenda);

/// The point that a leash order aims the creature at, from the place of the order or of the thing: x and z as a map
/// position keeps them, and two metres over the place, plus the height of the thing for an order to a thing
[[nodiscard]] glm::vec3 TargetPoint(glm::vec3 at, float thingHeight);

/// The number of the kind of a creature of the species in the game's tables of kinds, or nothing for no species
[[nodiscard]] std::optional<uint32_t> CreatureInfo(CreatureType species);

/// The agenda of the plan as the save keeps it. The save writes the steps from the step that the creature does to the
/// last. It writes no step when one of these steps does not keep the game's step.
[[nodiscard]] gamesave::Agenda AgendaToSave(const creature_mind::IdleMind& idle, const gamesave::ObjectRef& creature);

/// The record that a save writes for a creature that came from a file of the game. A value that openblack has not
/// changed since the load keeps the file's value, so a creature that did not change writes the same bytes as the file.
/// - loaded: the record that the file had.
/// - baseline: the record that openblack wrote from the creature at once after the load.
/// - now: the record that openblack writes from the creature now.
[[nodiscard]] gamesave::CreatureObject MergeOnto(const gamesave::CreatureObject& loaded,
                                                 const gamesave::CreatureObject& baseline, const gamesave::CreatureObject& now);

/// The leash record that a save writes for a leash that came from a file of the game, as MergeOnto does for the creature
[[nodiscard]] gamesave::LeashStatusObject MergeLeashOnto(const gamesave::LeashStatusObject& loaded,
                                                         const gamesave::LeashStatusObject& baseline,
                                                         const gamesave::LeashStatusObject& now);

/// Changes the keys of the references of the record as the table says. A key that is not in the table stays.
void Rekey(gamesave::CreatureObject& creature, const std::function<gamesave::ObjectKey(gamesave::ObjectKey)>& rekey);

} // namespace openblack::creature_save_state
