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

#include <vector>

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs
{
class Registry;
}

/// The list of the towns of each player, in the order of the game: a town goes to the end of the list of its player
/// when the land makes it, and again when another player wins it
namespace openblack::ecs::player_towns
{

/// The towns of a player, the first town that the player got first
[[nodiscard]] std::vector<entt::entity> InOrder(const Registry& registry, PlayerNames player);
/// How many towns a player has now
[[nodiscard]] size_t Total(const Registry& registry, PlayerNames player);

} // namespace openblack::ecs::player_towns
