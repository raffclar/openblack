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

/// The people about a death. Up to ten of them at once take notice, those of a town with no working graveyard: each
/// points at the dead person, talking, for a few seconds once it faces it, or goes to look; walking to a few metres off
/// the body, it turns to it, then mourns there for some seconds and goes back to what it was doing. A dead creature is
/// only watched. The state functions are the living action system's.
namespace openblack::ecs::villager_mourning
{

/// How urgently a living thing heeds a death, before its distance counts, from the reaction's priority: the dead thing
/// is the reaction's initiator, and the reaction's id says who already reacts to it
[[nodiscard]] uint8_t Priority(entt::entity living, entt::entity dead, uint32_t reaction, uint8_t priority);
/// A villager takes notice of a death
void Setup(components::LivingAction& action, entt::entity dead);

uint32_t PointAtDeadPerson(components::LivingAction& action);
uint32_t GoTowardsDeadPerson(components::LivingAction& action);
uint32_t LookAtDeadPerson(components::LivingAction& action);
uint32_t MournDeadPerson(components::LivingAction& action);
/// Leaving the mourning for anything but another reaction's state, it no longer reacts to the death
bool ExitMourning(components::LivingAction& action, VillagerStates next);
/// The mourning ends when the death's reaction has gone, or when the hand takes up the body and the reaction ends then
bool ValidateMourning(components::LivingAction& action);

} // namespace openblack::ecs::villager_mourning
