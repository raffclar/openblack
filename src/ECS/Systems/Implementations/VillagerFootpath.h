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
#include <glm/vec2.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// Villagers going to a building walk the footpaths that lead from it. A villager near one of the building's paths
/// joins it, walks it node by node and stops on its last node, beside the building; one farther off walks one of its
/// town storage pit's paths to the pit and the building's path on from there. Without a path to take it walks straight
/// to its goal, going round what stands in its way. The rules are footpath_rules'; this applies them to the villager.
/// Part of the living action system.
namespace openblack::ecs::villager_footpath
{

/// The villager sets off for a building, to a point of it, ending in a state: along the building's footpaths where one
/// leads its way, otherwise straight there. Whether it went.
bool SetupMoveToOnFootpath(components::LivingAction& action, entt::entity building, glm::vec2 goal, VillagerStates final);

/// Walking a footpath: on reaching each node it heads for the next, until the path ends and it goes into the state it
/// was heading for
uint32_t MoveOnPath(components::LivingAction& action);
/// It leaves the path behind
bool ExitMoveOnPath(components::LivingAction& action, VillagerStates next);

/// A building's footpaths go over to another building put up in its place, or to the plan a destroyed one leaves
void HandOverFootpaths(entt::entity from, entt::entity to);

} // namespace openblack::ecs::villager_footpath
