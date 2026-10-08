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

#include <functional>
#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "ECS/Components/LivingAction.h"
#include "ECS/Villager/VillagerEmergency.h" // SetStateWhenTappedOnAbode and 197 are declared there
#include "Enums.h"

namespace openblack
{
struct GVillagerInfo;
}

// The villager's home: going home (36), arriving (37), staying in (38), going to bed (119, 120, 121), the homeless
// (129), the vagrants (130), the tent next to a tree (238), the child that goes home to change (234), the at-home flag
// with Abode::presentAtHome, and the moves between abodes. Positions are MapCoords x / z (ecs::town_queries).

namespace openblack::ecs::villager
{
// ---- the villager's links ----------------------------------------------------------------------------------------

/// The villager's abode; the town cleared, then with an abode the abode's town
void SetAbode(entt::entity villager, entt::entity abode);
/// The villager's town
void SetTown(entt::entity villager, entt::entity town);
/// Inside its home (ArriveHome / LeaveHome)
[[nodiscard]] bool IsAtHome(entt::entity villager);
/// IsAvailable && !IsAtHome && not in the hand (fire::traits::InHand) && TOP != 236 GO_AND_HIDE_IN_NEARBY_BUILDING
[[nodiscard]] bool IsReachable(entt::entity villager);
/// Not controlled by a script, available for a state change (not in the hand), and the available-state flags of
/// GetFinalState's row & 1
[[nodiscard]] bool IsVillagerAvailable(entt::entity villager);
/// With an abode, the at-home flag is set and abode_villagers::ArriveHome (++presentAtHome). The flag is not tested
/// first: two calls count two (literal)
void ArriveHome(entt::entity villager);
/// When inside: clears the at-home and going-to-bed flags and, with an abode, abode_villagers::LeaveHome
void LeaveHome(entt::entity villager);

// ---- the state functions (LivingActionSystem.cpp k_VillagerStateTable) ------------------------------------------

/// DoGoingHome(37 ARRIVES_HOME, 238 SLEEP_IN_TENT). Always 1
uint32_t GoHome(entt::entity villager);
/// State 36 GO_HOME: GoHome
uint32_t GoHomeState(components::LivingAction& action);
/// With an abode, inside -> 38; already on the way -> nothing; else the walk to its door with FINAL `arrive`. Without
/// one: no town -> 130 VAGRANT_START; more than 100 m from the town -> a walk to 10..35 m from it on my side (FINAL the
/// TOP); else a tent near me (FINAL `tent`) or a stroll (FINAL the TOP). TODO(dance): leaving a dance first. Always 1
uint32_t DoGoingHome(entt::entity villager, VillagerStates arrive, VillagerStates tent);
/// Standing on the object's arrive point and going elsewhere -> SetupMoveToWithHug(pos, final); else the object's
/// footpath walk if necessary
void SetupMoveToOnFootpath(entt::entity villager, entt::entity object, glm::ivec2 pos, VillagerStates final);
/// (37, also 249 and from 35): not at the door -> the walk again (FINAL 37); built and repaired -> in; hurt: a
/// functional abode -> in, else a tent (238); hungry: in (with SetTopState(163) first when it is not functional,
/// literal); else SetupBuildingObject(abode) (VillagerBuild.h: its repair site) == 1 -> 1, or in. No abode -> 129 and 0
uint32_t ArrivesHome(entt::entity villager);
/// State 37 ARRIVES_HOME
uint32_t ArrivesHomeState(components::LivingAction& action);
/// State 38 AT_HOME: HomeDecideWhatToDo; 1
uint32_t AtHome(components::LivingAction& action);
/// The emergency -> 119; CheckNeedsAtHome; a disciple that ignores the needs (a BREEDER with Sleep first:
/// CheckSatisfySleep) -> DecideWhatToDo; CheckNeededForSomething; HomeNothingToDo and 0
uint32_t HomeDecideWhatToDo(entt::entity villager);
/// A woman's UpdatePregnancy / pregnancy; CheckSatisfyOwnDesire(0.9 x max(GetLifeDesireFromLife(D), POWER(F))) with (D, F)
/// = (DamageThresholdToSleepUntil, HungryForFood), or (DamageThresholdToGoHome, StarvingForFood) for a disciple that
/// ignores the needs; a child's CheckChildActivity (ChildDecideWhatToDo, always 1)
uint32_t CheckNeedsAtHome(entt::entity villager);
/// Inside and GameRand(4) == 0 -> counter 0, SetTopState(119); else SetupNothingToDo. 1
uint32_t HomeNothingToDo(entt::entity villager);
/// The exit of 35..38, 118..121, 125..127, 248, 249: the next state's StaysAtHomeOnExit == 0 -> LeaveHome. 1
uint32_t ExitAtHome(components::LivingAction& action, VillagerStates next);
/// State 119 GOTO_BED_AT_HOME: SetTopState(120), then the counter = RestAtHomeTime. 1
uint32_t GotoBedAtHome(components::LivingAction& action);
/// State 120 SLEEPING_AT_HOME: with a town --counter; at 0 DoSleeping(1) == 0 -> 38. 1
uint32_t SleepingAtHome(components::LivingAction& action);
/// (f): poisoned -> 0; life < info.life -> IncreaseLife(f x RestAtHomeRestoresLifeBy); Sleep (16) first in the town's
/// order 1, or life < DamageThresholdToSleepUntil -> counter = RestAtHomeTime, 1; else 0
uint32_t DoSleeping(entt::entity villager, float f);
/// State 121 WAKE_UP_AT_HOME: GoHome (no code sets 121)
uint32_t WakeUpAtHome(components::LivingAction& action);
/// (CheckSatisfySleep inside): once a stay (the going-to-bed flag); the old age's death -> 0; else the pair's
/// CheckGetPregnantAtHome and 1
uint32_t CheckWhenGoingToBed(entt::entity villager);
/// WillHousewifeGetPregnant -> HousewifeGetsPregnant (VillagerBirth.h); the last call's result, 0 when she will not
uint32_t CheckGetPregnantAtHome(entt::entity villager);
/// State 238 SLEEP_IN_TENT (also 250)
uint32_t SleepInTent(entt::entity villager);
uint32_t SleepInTentState(components::LivingAction& action);
/// State 129 HOMELESS_START: CheckHungry, CheckNeededForSomething, CheckHomelessMoveIntoAbode, else SetupNothingToDo. 1
uint32_t HomelessStart(components::LivingAction& action);
/// State 130 VAGRANT_START: a town of my tribe within 200 m that takes me -> 163; hurt -> a tent; else a stroll ahead
/// (FINAL 130). 1
uint32_t VagrantStart(entt::entity villager);
uint32_t VagrantStartState(components::LivingAction& action);
/// State 234 GO_HOME_AND_CHANGE: to the door (FINAL 234, a direct walk); there -> 37 or 38 (inside); no abode -> 163;
/// then a scale below 0.95 -> SetScaleForAge. 1
uint32_t GoHomeAndChange(components::LivingAction& action);
/// Another exit -> ChangeTribeIfRequired(the town's tribe, else the info's, the next state's StaysAtHomeOnExit == 0);
/// CHANGE_HOUSE disciple -> SetVillagerDisciple(0) (VillagerDisciple.h). 1
uint32_t ExitGoHomeAndChange(components::LivingAction& action, VillagerStates next);

// ---- the abode and the town --------------------------------------------------------------------------------------

/// A town and FindAbodeWithSpaceInTown(me, 0) -> out of the homeless list, AddVillagerToAbode, SetTopState(36); 1. Else
/// 0
uint32_t CheckHomelessMoveIntoAbode(entt::entity villager);
/// MakeHomelessNoStateChange and SetTopState(129); its result
bool MakeHomeless(entt::entity villager);
/// Out of its abode (SetAbode(null), SetTown(town)); no town -> 0; already in the list -> 0; out of the vagrants; at
/// the head of the town's homeless list; 1
bool MakeHomelessNoStateChange(entt::entity villager);
/// Its abode is destroyed: the original also clears another link of the villager to it (TODO: not identified); an abode
/// -> MakeHomeless; else the town's deletion (TODO: villager death)
void HomeDeleted(entt::entity villager);
/// A child -> 0; an abode that is not too crowded -> 0; no town -> VagrantStart, 1; a better abode
/// (FindAbodeWithSpaceInTown above the current score) and MoveVillagerToAbode -> 36 if available, 1; else MakeHomeless
/// unless already in the list, 1
uint32_t CheckNeedNewAbode(entt::entity villager);
/// Room left (children / adults, signed) > 0 -> ForceMoveVillagerToAbode, 1; else 0
uint32_t MoveVillagerToAbode(entt::entity villager, entt::entity abode);
/// The same town -> AddVillagerToAbode; else the old town's RemoveVillager and, below 100% full (children / adults),
/// AddVillagerToAbode, else the new town's AddVillagerToTown
void ForceMoveVillagerToAbode(entt::entity villager, entt::entity abode);
/// (tribe, leaving): KeepMeshWhenChangeTown == 0 -> ChangeInfo(FindVillagerInfo(tribe, my number)) and, leaving, a puff
/// of smoke (disappear_smoke::Create(point, 1, 1.0, -1))
void ChangeTribeIfRequired(entt::entity villager, Tribe tribe, bool leaving);
/// The info is set; a child the three meshes = ChildMeshHigh, an adult the detail meshes 2 / 1 / 0: the only place a
/// grown-up child gets its adult mesh. 1
uint32_t ChangeInfo(entt::entity villager, const GVillagerInfo& info);
/// The villager's mesh (Mesh, or SkeletalAnimation::hiddenMesh while it is not drawn): a child ChildMeshMedium (as
/// SetAge sets them), or ChildMeshHigh when `childHighOnly` (ChangeInfo's); an adult StdDetail
void SetVillagerMeshes(entt::entity villager, const GVillagerInfo& info, bool child, bool childHighOnly);
/// The FIRST info record of that tribe and number; nullptr when none
[[nodiscard]] const GVillagerInfo* FindVillagerInfo(Tribe tribe, VillagerNumber number);
/// (abode; entt::null -> its own): door + GetPosFromAngle(Get3DAngleFromXZ(abode, door) + (pi/8 - GameFloatRand(pi/4)),
/// GameFloatRand(1.5) + 1.5)
[[nodiscard]] glm::ivec2 FindPosOutsideAbode(entt::entity villager, entt::entity abode);
/// The villager's town: a valid town entity with its Town component, else entt::null. The exported copy
/// (VillagerHome.cpp's TownOf; VillagerEmergency uses it). VillagerBuild / VillagerFarmer / VillagerFisherman /
/// VillagerForester still keep private copies (a separate cleanup)
[[nodiscard]] entt::entity GetTown(entt::entity villager);
// SetupBuildingObject(abode): villager::SetupBuildingObjectForBuilding in VillagerBuild.h
/// StartHavingSexAge <= age < StopHavingSexAge
[[nodiscard]] bool IsSexuallyActive(entt::entity villager);

// ---- the tent ----------------------------------------------------------------------------------------------------

/// (pos&): the nearest tree within 50 m with room (TentNextToTree) -> 2 m from it; else 3 tries near `pos`: a clear
/// cell (collide & 0x19 == 0) whose 9 spiral cells (the spiral moves `pos` itself) have no villager in 238 within 5 m
/// -> pos 9 spiral steps away; a failed try moves pos by GetPosFromAngle(GameFloatRand(2 pi), GameFloatRand(5) + 3)
/// (the 5 first). 0 / 1
bool GetTentPos(entt::entity villager, glm::ivec2& pos);
/// The nearest tree in a spiral: map_cells::FindNearestInSpiral with IsTree as the filter
[[nodiscard]] entt::entity FindNearestTree(glm::ivec2 pos, float radius);
/// (tree, villager, &out): the 9 spiral cells from the tree; a villager in 238, or a MultiMapFixed
/// (map_cells::IsMultiCellStaticClass), within distance - Get2DRadius < 4 of the tree is an occupant; a second one -> 0.
/// the point is the tree + 2 m away from the occupant (or towards the villager)
[[nodiscard]] std::optional<glm::ivec2> TentNextToTree(entt::entity tree, entt::entity villager);
} // namespace openblack::ecs::villager
