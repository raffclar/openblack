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

#include <entt/entity/fwd.hpp>

#include "3D/MapCoords.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A villager in one of the scripts' flocks, keeping up with it
namespace openblack::ecs::villager_flock
{

/// What stands in a map cell for something walking there, as the game's collide types: off the map the edge, else
/// water or land, and the trees and fields in it
[[nodiscard]] uint32_t CellCollision(const map_coords::MapCoords& coords);
/// Whether the villager can stand at a point: on the map, and on nothing its kind can't walk on
[[nodiscard]] bool CanStandAt(entt::entity villager, const map_coords::MapCoords& coords);

/// Each turn in its flock: it waits within its flock's domain, and outside it walks back towards it or its leader
uint32_t MoveInFlock(components::LivingAction& action);

} // namespace openblack::ecs::villager_flock
