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

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// The piles the food and wood miracles (and the hand) leave on the land: magic food (MagicFood.cpp) and magic wood
// (MagicWood.cpp). Wiki: docs/bw1-notes/miracles.md, "Food and wood".

namespace openblack::magic::objects
{
/// FOOD -> CreateMagicFood, WOOD -> CreateMagicWood; any other type none. position: x, z on the map, y ignored (the
/// pile stands on the land). player nullopt = none. allowEmpty: an amount of 0 still makes the pile (the original pot
/// creation has no such check; the town's temporary resource store pots are made empty). Without it PotArchetype's own
/// rule (no pile for 0) applies
entt::entity CreateMagicResourcePile(const glm::vec3& position, std::optional<PlayerNames> player, ResourceType type,
                                     uint32_t amount, bool allowEmpty = false);

/// A food pile of pot info 10 "Magic Food" (MSH_S_GRAIN_PILE), owned by the player (none -> the neutral player), at
/// scale 0.3
entt::entity CreateMagicFood(const glm::vec3& position, std::optional<PlayerNames> player, uint32_t amount,
                             bool allowEmpty = false);

/// A wood pile of pot info 9 "Magic Wood" (MSH_B_WOOD_01), owned by the player, at scale 0.7
entt::entity CreateMagicWood(const glm::vec3& position, std::optional<PlayerNames> player, uint32_t amount,
                             bool allowEmpty = false);
} // namespace openblack::magic::objects
