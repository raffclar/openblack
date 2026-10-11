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

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A villager sent for food: it weighs its town's fish farms, fields and flocks against taking what it carries to the
/// store. A fisherman walks to the farm, steps to a spot round it, casts with his rod, and on each cast may catch fish
/// out of the sea; once he has less room than a catch he takes his bag to the store. The state functions are the living
/// action system's.
namespace openblack::ecs::villager_fisherman
{

/// The town wants food: the villager takes up the best food job for it, or takes what it carries to the store. 1 when
/// it set about something.
uint32_t CheckSatisfyFoodDesire(components::LivingAction& action);
/// The villager looks for one of its town's fish farms that wants a fisherman and walks there. 1 when it went.
uint32_t FishermanLookForWater(components::LivingAction& action);
/// The villager walks to a fish farm to fish it. 1 when it went.
uint32_t VillagerBecomesFisherman(components::LivingAction& action, entt::entity farm);

uint32_t FishermanArrivesAtFishing(components::LivingAction& action);
uint32_t Fishing(components::LivingAction& action);
/// Taking up fishing from another kind of state it joins the farm's fishermen, and leaving for another kind it leaves
/// them
bool EnterFishing(components::LivingAction& action, VillagerStates previous, VillagerStates next);
bool ExitFishing(components::LivingAction& action, VillagerStates next);

} // namespace openblack::ecs::villager_fisherman
