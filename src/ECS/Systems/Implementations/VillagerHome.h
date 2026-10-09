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
#include <glm/vec2.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A villager's day around its home: deciding what to do, going home, staying in and sleeping, sitting about, lying
/// down in the open when it has no home to go to, gathering in an emergency, and coming out when its home is knocked on
/// or hurt. Villagers take up what their town wants most that they can serve; at night that is sleep, which sends them
/// home to bed. The state functions are the living action system's.
namespace openblack::ecs::villager_home
{

/// Walks the villager to a point, then on into a state
void SetupMoveTo(components::LivingAction& action, glm::vec2 goal, VillagerStates final);
/// The villager's walk to a goal starts afresh, ending in a final state, whatever state it is in meanwhile
void SetupMobileMoveTo(components::LivingAction& action, glm::vec2 goal, VillagerStates final);

/// The villager goes into its abode, and isn't drawn while there
void ArriveHome(entt::entity villager);
/// It comes out again
void LeaveHome(entt::entity villager);

/// Whether a building works: built, and not damaged past the share at which it stops working
[[nodiscard]] bool IsFunctional(entt::entity abode);
/// Where a villager goes into its abode: the model's door, or the abode itself without one
[[nodiscard]] glm::vec2 ArrivePosition(entt::entity abode);

/// Its home was knocked on or hurt: a villager inside comes out a few steps from the door, yawns and decides afresh,
/// and won't go back to bed for its next decision. Whether it came out.
bool SetStateWhenTappedOnAbode(entt::entity villager);

/// Sends the villager to bed if it is home, or home if it has one, unless it was just knocked out of it. 1 when it went.
uint32_t CheckSatisfySleep(components::LivingAction& action);
/// Whether the villager is wanted for something: a home if it has none, its town's desires, its own needs
uint32_t CheckNeededForSomething(components::LivingAction& action);
/// What a villager with nothing to do does: goes home, sits outside it, or sits about its town
uint32_t SetupNothingToDo(components::LivingAction& action);
/// What a villager in its home does next: bed in an emergency, its own needs, its town's, or nothing much
uint32_t HomeDecideWhatToDo(components::LivingAction& action);

uint32_t DecideWhatToDo(components::LivingAction& action);
uint32_t MoveToPos(components::LivingAction& action);
uint32_t GoHome(components::LivingAction& action);
uint32_t ArrivesHome(components::LivingAction& action);
uint32_t AtHome(components::LivingAction& action);
uint32_t GotoBedAtHome(components::LivingAction& action);
uint32_t SleepingAtHome(components::LivingAction& action);
uint32_t HomelessStart(components::LivingAction& action);
uint32_t VagrantStart(components::LivingAction& action);
uint32_t SleepInTent(components::LivingAction& action);
uint32_t AfterTapOnAbode(components::LivingAction& action);
uint32_t NothingToDo(components::LivingAction& action);
uint32_t GoAndChilloutOutsideHome(components::LivingAction& action);
uint32_t GoAndChilloutInTown(components::LivingAction& action);
uint32_t SitAndChillout(components::LivingAction& action);
bool EnterSitAndChillOut(components::LivingAction& action, VillagerStates previous, VillagerStates next);
uint32_t GotoCongregateInTownAfterEmergency(components::LivingAction& action);
uint32_t CongregateInTownAfterEmergency(components::LivingAction& action);
/// Leaving a home state: the villager comes out unless the next state keeps it inside
bool ExitAtHome(components::LivingAction& action, VillagerStates next);

} // namespace openblack::ecs::villager_home
