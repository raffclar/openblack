/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
} // namespace openblack::ecs::components

// The villagers and the worship site: the town's worship percentage sends them to the site's arrive point (59), up to
// maxDancersVisible (20) dance (60) and the rest wait at the hide point (213); the chants they make cost them life
// (ReduceVillagerLifeByChant); when fewer are needed the first of the go-home queue leaves (248). The movement is
// openblack's (the WallHug pathfinding): the original walks on the footpath to the site and to its dance place.

namespace openblack::ecs::villager_worship
{
/// The idle check (DECIDE_WHAT_TO_DO): already at the site -> start again; else, if the town worships and
/// GetWorshipersNeeded(1, 1) > 0 -> CheckWorshipActivity. 1 when it went.
bool CheckNeededForWorship(entt::entity villager);
/// A worship site, the town centre functional and built, the site's player is the town's; a villager that cannot get
/// there (CanIGetToTheWorshipSite: within maxDistanceThatVillagersWillGoToWorship, 500) only goes when not
/// `requireReachable` -> GotoWorshipSiteForWorship. A site farther than that is still reachable through the player's
/// teleport stones (teleport::FindRouteStone): then the villager also starts reacting to the stone it must walk to,
/// so the walk to the site goes through two stones
bool CheckWorshipActivity(entt::entity villager, bool requireReachable);
/// IsVillagerAvailable (the state table's availability bit field0xa8 & 1), not flagged on the first pass, and not
/// IsAtOrOnTheWayToWorshipSite
[[nodiscard]] bool IsAvailableForWorshipSite(entt::entity villager, bool secondPass);
/// Flagged at the site, or its state 59 / 46
[[nodiscard]] bool IsAtOrOnTheWayToWorshipSite(entt::entity villager);
/// SetState(163 DECIDE_WHAT_TO_DO), as the town's worshipper adjustment sends them back
void SendBackToTown(entt::entity villager);
/// At the worship site (components::WorshipVillager::atSite)
[[nodiscard]] bool IsAtWorshipSite(entt::entity villager);
/// Off the town's worshipper count when at the site, off the site's count and out of its dance, the at-site flag
/// cleared. Sets no state. Called by the worship states and when the town removes the villager
void RemoveVillagerFromWorshipSite(entt::entity villager);

// the state table entries (LivingActionSystem.cpp k_VillagerStateTable)
uint32_t GotoWorshipSiteForWorshipState(components::LivingAction& action);         ///< 58 (the walk, resumed)
uint32_t ArrivesAtWorshipSiteForWorship(components::LivingAction& action);         ///< 59 (after the walk)
uint32_t WorshippingAtWorshipSite(components::LivingAction& action);               ///< 60
uint32_t HidingAtWorshipSite(components::LivingAction& action);                    ///< 213
bool ExitMoveToWorshipSite(components::LivingAction& action, VillagerStates next); ///< 58, 59
bool ExitAtWorshipSite(components::LivingAction& action, VillagerStates next);     ///< 60, 213
} // namespace openblack::ecs::villager_worship
