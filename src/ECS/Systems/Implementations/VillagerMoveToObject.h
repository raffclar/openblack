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

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A villager walking up to a thing that may move, as a shepherd walks up to its flock's leader. Each turn it heads for
/// where the thing is now, going a quarter faster than the thing should it be slower, until it is within a step of it;
/// then it goes into the state it was heading for. Should the thing go, it decides what to do again.
namespace openblack::ecs::villager_move_to_object
{

/// The villager sets off for a thing, ending in a state. Whether the states let it go.
bool SetupMoveToObject(components::LivingAction& action, entt::entity thing, VillagerStates final);
uint32_t MoveToObject(components::LivingAction& action);

/// Where a villager stands to work at a thing, as a map position. By a tree, it stands its own size and a little more
/// from the trunk. By any other thing, it stands both their sizes from the middle of the thing. The goal has the map
/// position of the thing plus a step towards the villager, in whole map units, and the height of the thing above the land.
[[nodiscard]] map_coords::MapCoords WorkingPosition(entt::entity thing, entt::entity villager);

} // namespace openblack::ecs::villager_move_to_object
