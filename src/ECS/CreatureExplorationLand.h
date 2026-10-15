/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "Creature/CreatureExplorationMap.h"

/// The creatures' map of the land on the land as it is: the regions made from the land's cells and its towns, the
/// regions that each creature went into, and the places that creatures find with them
namespace openblack::ecs::creature_exploration_land
{

/// Makes the regions of the land again, from the land's cells and the buildings of the towns that stand on it now. The
/// game does this when the land and its towns are ready.
void MakeMap();
/// The regions of the land. When nothing made them yet, this makes them first. None without a land.
[[nodiscard]] const creature_exploration::RegionMap* Map();
/// Notes that the creature went into the region of the place
void Visit(entt::entity creature, glm::vec2 place);
/// The place in the nearest region of the kind from a place, that the creature goes to. With preferUnvisited, the
/// creature prefers a region that it did not go into. None when there is no such region, or no land.
[[nodiscard]] std::optional<glm::vec2> FindPlace(entt::entity creature, creature_exploration::Kind kind, glm::vec2 from,
                                                 bool preferUnvisited);

} // namespace openblack::ecs::creature_exploration_land
