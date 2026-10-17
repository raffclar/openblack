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
#include <optional>

#include <CreatureRecord.h>
#include <entt/entity/entity.hpp>

#include "Creature/CreatureFizz.h"
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

/// The reference that the saved game keeps for an entity: 0 for none
[[nodiscard]] gamesave::Ref ToRef(entt::entity entity);
[[nodiscard]] gamesave::Ref ToRef(std::optional<entt::entity> entity);
[[nodiscard]] std::optional<entt::entity> FromRef(gamesave::Ref ref);

/// Whether the save keeps the plan and its agenda
[[nodiscard]] bool KeepsPlan(bool scriptControlled, bool fighting, size_t dancers);

[[nodiscard]] gamesave::SpellSlot ToSlot(const creature_spells::Slot& slot);
[[nodiscard]] creature_spells::Slot FromSlot(const gamesave::SpellSlot& slot);
[[nodiscard]] gamesave::QueuedSpell ToQueued(const creature_spells::Waiting& waiting);
/// A queued spell gets its turns from its seconds, as when the game starts it
[[nodiscard]] std::optional<creature_spells::Waiting> FromQueued(const gamesave::QueuedSpell& queued, float turnsPerSecond);

/// The fizz that the save writes: the creature's fizz while the invisibility spell is on it, else none
[[nodiscard]] creature_fizz::Fizz FizzToSave(const creature_fizz::Fizz& fizz, const creature_spells::Spells& spells);

/// The leash types that a creature knows, as the save keeps them
[[nodiscard]] gamesave::KnownLeashes ToKnown(const std::bitset<creature_leash::k_Types.size()>& known);
[[nodiscard]] std::bitset<creature_leash::k_Types.size()> FromKnown(const gamesave::KnownLeashes& known);

/// The number that the save keeps for a player, k_None for none
[[nodiscard]] uint8_t PlayerNumber(std::optional<PlayerNames> player);
[[nodiscard]] std::optional<PlayerNames> PlayerFromNumber(uint8_t number);

/// The leash type as the save keeps it, and back: 1 evil, 2 the learning leash, 3 good
[[nodiscard]] uint32_t LeashTypeNumber(LeashType type);
[[nodiscard]] std::optional<LeashType> LeashTypeFromNumber(uint32_t number);

/// Whether a load puts the creature back on the map, from the size of its mind file (none when the file does not open).
/// Only a mind file that opens and is not empty does this.
[[nodiscard]] bool ResolvesOnLoad(std::optional<size_t> mindFileSize);

/// The agenda that a load gives back: the game starts the step that the creature did again from its beginning
[[nodiscard]] gamesave::Agenda AgendaAfterLoad(gamesave::Agenda agenda);

} // namespace openblack::creature_save_state
