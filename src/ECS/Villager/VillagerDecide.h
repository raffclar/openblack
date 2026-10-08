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
#include "ECS/Villager/VillagerFood.h"
#include "ECS/Villager/VillagerHome.h"
#include "Enums.h"

// The villager's decision (state 163 DECIDE_WHAT_TO_DO) and its idle states (209 NOTHING_TO_DO, 245
// GO_AND_CHILLOUT_OUTSIDE_HOME, 246 SIT_AND_CHILLOUT, 252 GO_AND_CHILLOUT_IN_TOWN), as the original does them
// (docs/bw1-notes/villagers.md). The checks not ported yet are neutral (they answer 0 / "no"), never
// approximated. Positions are MapCoords x / z (ecs::town_queries).

namespace openblack::ecs::villager
{
// ---- the state functions (LivingActionSystem.cpp k_VillagerStateTable) ------------------------------------------

/// 163 DECIDE_WHAT_TO_DO. Always 1
uint32_t DecideWhatToDo(components::LivingAction& action);
/// 209: does nothing. Always 1
uint32_t NothingToDo(components::LivingAction& action);
/// 245 GO_AND_CHILLOUT_OUTSIDE_HOME
uint32_t GoAndChilloutOutsideHome(components::LivingAction& action);
/// 246 SIT_AND_CHILLOUT
uint32_t SitAndChillout(components::LivingAction& action);
/// 246's entry: the counter = initialChillOutTime; 1
uint32_t EnterSitAndChillOut(components::LivingAction& action, VillagerStates final, VillagerStates next);
/// 252 GO_AND_CHILLOUT_IN_TOWN (only a script sets 252)
uint32_t GoAndChilloutInTown(components::LivingAction& action);
/// 114 (VillagerChild.cpp): the child's checks, then a walk (FINAL 114) to a
/// point 5 m from its mother, or from its abode
uint32_t ChildFollowsMother(components::LivingAction& action);

// ---- DecideWhatToDo's checks ---------------------------------------------------------------------------------------

/// Homeless -> CheckHomelessMoveIntoAbode (VillagerHome.h); then
/// CheckNeededForSpecial == 1
uint32_t CheckNeededForSomething(entt::entity villager);
/// Worship (CheckNeededForWorship), civic, own desires
uint32_t CheckNeededForSpecial(entt::entity villager);
/// With a town, CheckNeededForTownDesire == 1
uint32_t CheckNeededForCivic(entt::entity villager);
/// GetOwnDesiresTrigger, the town desire's
/// CheckVillagerNeededForTownDesire (town_desire; 0 or 1) and flags &= ~1
uint32_t CheckNeededForTownDesire(entt::entity villager);
[[nodiscard]] float GetOwnDesiresTrigger(entt::entity villager);
/// GetLifeDesireFromLife(GetLife())
[[nodiscard]] float GetDesireForLife(entt::entity villager);
/// 1 - ((life - min(D, life)) / (1 - D))^2, D = damageThresholdToGoHome
[[nodiscard]] float GetLifeDesireFromLife(entt::entity villager, float life);
/// The larger of food and life desire (minus the trigger) served first
uint32_t CheckSatisfyOwnDesire(entt::entity villager, float trigger);
/// CheckSatisfyOwnFoodDesire and ChangeStateToFindFoodToEat are in VillagerFood.h
/// Inside its home: CheckWhenGoingToBed (VillagerHome.h), then 119
uint32_t CheckSatisfySleep(entt::entity villager);
/// Wood held > minWoodToShowGraphic or food held >
/// minFoodToShowGraphic (signed) -> SetTopState(31); 1
uint32_t CheckTakeResourcesToStoragePit(entt::entity villager);
/// DiscipleDecideWhatToDo is in VillagerDisciple.h
/// CheckChild, CheckNeededForTownDesire, ChildGotoCreche, else 114. Always 1
uint32_t ChildDecideWhatToDo(entt::entity villager);
uint32_t CheckChild(entt::entity villager);
/// IsMotherAlive and ChildGotoCreche are in VillagerChild.h
/// CheckNeedNewAbode is in VillagerHome.h

// ---- the idle branch ---------------------------------------------------------------------------------------------

/// GameRand(9) picks the branch (0, 1, 1, 1, 2, 2, 2, 2, 2). Always 1
uint32_t SetupNothingToDo(entt::entity villager);
/// Around the town's congregation point, on my side (+-22.5 degrees), R to 10 R
/// away (R = 0.1 x the town's maxDistanceFromCongreationPosThatPeopleChillOut). None without a town
[[nodiscard]] std::optional<glm::ivec2> GetChillOutPos(entt::entity villager);
/// The abode's GetPosOutside(3, 0.5 R, 0.5 R), R = the town's maxDistanceFromHouseThatPeopleChillOut. 0 without a
/// town or an abode (none then)
[[nodiscard]] std::optional<glm::ivec2> GetPosOutsideMyHouse(entt::entity villager);
/// The member function GetMeToMyChillOutPos takes (GetPosOutsideMyHouse or GetChillOutPos)
using ChillOutPosFn = std::optional<glm::ivec2> (*)(entt::entity villager);
/// (pmf, A, R, C): far from A (> R) walk to pmf's point; near: sit (246) where
/// it is clear (CheckForClearArea, IsObject, 1.2 x my radius), turning one step to C (LookAtPos(C, 2)); else walk to a
/// clear point nearby (FindClearArea 5, 1) or to pmf's point. Every walk keeps GetFinalState as its final state
void GetMeToMyChillOutPos(entt::entity villager, ChillOutPosFn pmf, glm::ivec2 a, float r, const glm::ivec2* c);

// ---- test hooks --------------------------------------------------------------------------------------------------

/// OPENBLACK_TEST_VILLAGER_NOTHING: the next SetupNothingToDo of `villager` takes `r` for its GameRand(9) (the draw is
/// still made, so the order of the draws does not change). Puts a components::ForcedNothingRoll on the villager
void ForceNextNothingRoll(entt::entity villager, uint32_t r);
/// Each check DecideWhatToDo calls is published as an events::VillagerDecideStep, in order
} // namespace openblack::ecs::villager
