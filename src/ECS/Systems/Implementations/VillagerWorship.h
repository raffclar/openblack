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

#include "ECS/WorshipperRules.h"
#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A town's people at worship. The town's totem sets the share of its people wanted at its worship site; the town
/// sends the fittest of its free people there (and every ten turns sends more if it is short), and calls the weakest
/// back first when too many are there. A worshipper walks to the site's gate, joins the dance, or worships hidden in the
/// site when the dance is full, chants (which costs it life and can kill it), eats from the site's pot when hungry, and
/// goes home when its town has more worshippers than it wants. The state functions are the living action system's.
namespace openblack::ecs::villager_worship
{

/// The town's worship counts and share, for the rules
[[nodiscard]] worshipper_rules::Headcount HeadcountOf(entt::entity town);
/// How many more people a town wants at worship, as the rules count it
[[nodiscard]] worshipper_rules::Needed WorshippersNeeded(entt::entity town, bool countOnTheWay, bool makeUpForThoseGoingHome);
/// Sends more of a town's people to worship (a count above none) or calls some back (below none): the fittest first
/// and the least fit first, those of no discipline before disciples. Low life stops none from going when ignored; a
/// villager that can't reach the site goes only when reaching isn't needed.
void AdjustWorshippers(entt::entity town, int32_t count, bool ignoreLowLife, bool needReach);
/// A town's share has just been set: it sends or calls back people to match it
void ShareSet(entt::entity town);
/// The town's turn: every tenth turn it sends people to worship when it has too few, never calling any back
void TownTurn(entt::entity town, uint32_t turn);
/// A villager taken from the world is forgotten by the towns and sites counting it
void Forget(entt::entity villager);
/// A villager leaving its town alive is no longer counted among those on their way to the town's worship site, and one
/// counted at worship leaves the site and its dance
void LeaveTown(entt::entity villager);

/// Whether a villager is worshipping or on its way to
[[nodiscard]] bool IsAtOrOnTheWayToWorshipSite(entt::entity villager);
/// A villager's town's worship site, none without a town or a site
[[nodiscard]] entt::entity WorshipSiteOf(entt::entity villager);

/// Whether the villager is wanted at worship, and goes: one already counted at worship goes back to the dance. 1 when it
/// went.
uint32_t CheckNeededForWorship(components::LivingAction& action);
/// The town wants the villager at worship: it builds the site if it is still going up, else sets off for it, unless
/// reaching the site is needed and it can't. Whether it went.
bool CheckWorshipActivity(components::LivingAction& action, bool needReach);

uint32_t GotoWorshipSiteForWorship(components::LivingAction& action);
uint32_t ArrivesAtWorshipSiteForWorship(components::LivingAction& action);
uint32_t WorshippingAtWorshipSite(components::LivingAction& action);
uint32_t HidingAtWorshipSite(components::LivingAction& action);
uint32_t RestartWorshippingAtWorshipSite(components::LivingAction& action);
uint32_t GetFoodAtWorshipSite(components::LivingAction& action);
uint32_t GotoAltarForRest(components::LivingAction& action);
uint32_t ArrivesAtAltarForRest(components::LivingAction& action);
uint32_t AtAltarRest(components::LivingAction& action);
uint32_t AtAltarFinishedRest(components::LivingAction& action);
uint32_t GoHomeFromWorship(components::LivingAction& action);

/// Fetching food from the storage pit for the worship site. The game fetches it but never carries it on to the site:
/// those who would set off with it give up instead.
uint32_t CheckSatisfySupplyWorship(components::LivingAction& action);
uint32_t GotoStoragePitForWorshipSupplies(components::LivingAction& action);
uint32_t ArrivesAtStoragePitForWorshipSupplies(components::LivingAction& action);
uint32_t GotoWorshipSiteWithSupplies(components::LivingAction& action);
uint32_t MoveToWorshipSiteWithSupplies(components::LivingAction& action);
uint32_t ArrivesAtWorshipSiteWithSupplies(components::LivingAction& action);

/// Leaving the way to the site for a state not on it (nor into a teleport stone): no longer on its way
bool ExitMoveToWorshipSite(components::LivingAction& action, VillagerStates next);
/// Leaving the site's dance or hiding place: for anything but a reaction or eating at the site it is no longer counted
/// at worship; either way it leaves the site's worshippers and its dance
bool ExitAtWorshipSite(components::LivingAction& action, VillagerStates next);
/// Leaving the site's pot for a state that doesn't keep it at the site: no longer counted at worship
bool ExitGetFoodAtWorship(components::LivingAction& action, VillagerStates next);
/// Leaving the supplies' states: the food it holds is no longer counted on its way to the site
bool ExitMoveToWorshipSiteWithSupplies(components::LivingAction& action, VillagerStates next);

} // namespace openblack::ecs::villager_worship
