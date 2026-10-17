/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <entt/entity/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::town_turns
{

/// A player's towns in the order the player gained them, which is the order they take their turns in
[[nodiscard]] std::vector<entt::entity> TownsInGainedOrder(const Registry& registry, PlayerNames player);

} // namespace openblack::ecs::town_turns
