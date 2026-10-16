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
#include <vector>

#include "Creature/CreatureIdleMind.h"

/// The thefts that a creature copies from its player: it takes a stolen thing to a town of its player, or it takes food
/// or wood from the store of a town of another player to a store of its own player. The rules and the agendas here are
/// pure, free of the game.
namespace openblack::creature_town_theft
{
using creature_mind::Step;

/// A storage pit belongs to the player of the creature when it is built and it has the same player as the creature. A
/// thing of no player has the same player as a creature of no player.
[[nodiscard]] bool IsOwnStoragePit(bool built, std::optional<uint32_t> pitPlayer, std::optional<uint32_t> creaturePlayer);
/// A storage pit belongs to another player when it is built and its player is not the player of the creature. A pit of
/// a neutral town, or of no town, also belongs to another player.
[[nodiscard]] bool IsOthersStoragePit(bool built, std::optional<uint32_t> pitPlayer, std::optional<uint32_t> creaturePlayer);
/// A field belongs to another player when its player is not the player of the creature. The field can be empty.
[[nodiscard]] bool IsOthersField(std::optional<uint32_t> fieldPlayer, std::optional<uint32_t> creaturePlayer);

/// The nearest of the towns of the player of the creature, from the distance of each town to the creature in metres.
/// Of two towns at the same distance, the first is the nearest. None when the player has no town.
[[nodiscard]] std::optional<size_t> NearestTown(std::span<const float> metres);

/// How near to the storage pit of the town the creature goes to put down a stolen thing: three times its height
inline constexpr float k_PutInTownHeights = 3.0f;

/// The creature steals a thing and takes it to a town of its player. These are the steps:
/// 1. When its hand is empty, it picks up the thing (pickUp) and looks at the thing as it goes to it.
/// 2. It goes near the storage pit of the nearest town of its player, to within k_PutInTownHeights of its height. It
///    looks at the pit as it goes.
/// 3. It puts the thing down.
/// The agenda has no main step.
[[nodiscard]] std::vector<Step> StealAndPutInTown(std::optional<uint32_t> pickUp, uint32_t pit, float height);

/// The time for which the creature turns to face a store
inline constexpr float k_FaceStoreSeconds = 0.1f;

/// The creature steals food or wood from the storage pit of another player. These are the steps:
/// 1. It goes near the pit that it steals from, and looks at it as it goes. Then it turns to face the pit.
/// 2. It takes a handful of the look that it steals (wheat for food, logs for wood) from the pit. The pit that it
///    steals from loses nothing.
/// 3. It goes to a place to throw from, to within its height of its own store, with a compassionate face. It looks at
///    the store.
/// 4. It turns to face the store, and the action is complete.
/// 5. The main step: it throws the handful into its own store.
[[nodiscard]] std::vector<Step> StealFromStore(uint32_t from, uint32_t into, creature_mind::ObjectOrder::Handful look,
                                               float height);
} // namespace openblack::creature_town_theft
