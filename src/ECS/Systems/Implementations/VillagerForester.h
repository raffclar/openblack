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

#include <optional>

#include <entt/entity/entity.hpp>

#include "ECS/BuildingSiteRules.h"
#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A villager fetching wood for its town or a building site: it weighs taking what it carries to the store against
/// walking to the nearest of its town's forests, walks there with its axe, finds the nearest forest tree in the cells
/// round it, turns, swings and fells it, watches it come down, then picks the wood up as anyone does wood lying about,
/// and carries the log home. A big forest gives its wood straight away and shrinks. The state functions are the living
/// action system's.
namespace openblack::ecs::villager_forester
{

/// Where a villager would get wood: the store, a big forest or a forest of its town, or nowhere
struct WoodPlan
{
	building_site::WoodSource source {building_site::WoodSource::None};
	entt::entity forest {entt::null};
	entt::entity bigForest {entt::null};
};
/// Where a villager gets wood, for building (a store with more wood than it can carry wins over a forest) or for its
/// town (by how full it is)
[[nodiscard]] WoodPlan DecideHowToGetWood(entt::entity villager, bool forBuilding);

/// The town wants wood: the villager takes what it carries to the store, or walks to a forest or big forest. 1 when it
/// went.
uint32_t CheckSatisfyWoodDesire(components::LivingAction& action);
/// Walks the villager with its axe to a forest, to stand by the tree in its middle, then on into a state
uint32_t GotoForest(components::LivingAction& action, entt::entity forest, VillagerStates final);
/// Walks the villager to where a big forest gives its wood, then on into a state
uint32_t GotoBigForest(components::LivingAction& action, entt::entity bigForest, VillagerStates final);
/// What a villager that got wood does next: back to deciding, on to its building site, to a site that wants builders,
/// or to the store
uint32_t GotWoodDecideWhatToDo(components::LivingAction& action);

uint32_t ForesterMoveToForest(components::LivingAction& action);
uint32_t ForesterGotoForest(components::LivingAction& action);
uint32_t ForesterArrivesAtForest(components::LivingAction& action);
uint32_t ForesterChopsTree(components::LivingAction& action);
uint32_t ForesterChopsTreeForBuilding(components::LivingAction& action);
uint32_t ForesterFinishedForestering(components::LivingAction& action);
uint32_t ArrivesAtBigForest(components::LivingAction& action);
uint32_t ArrivesAtBigForestForBuilding(components::LivingAction& action);
/// Leaving the forester's states it lets go of the tree it felled
bool ExitForesting(components::LivingAction& action, VillagerStates next);

uint32_t GotoWoodReaction(components::LivingAction& action);
uint32_t ArrivesAtWoodReaction(components::LivingAction& action);
/// A villager takes up wood it reacted to and remembers it
void SetupReactToWood(entt::entity villager, entt::entity wood);
/// How much a villager heeds wood lying about: the reaction's priority when the wood catches its eye, else 0
[[nodiscard]] uint32_t ReactToWoodPriority(entt::entity villager, entt::entity wood, uint32_t priority);
/// Whether a villager reacts again at once to wood it reacted to before: a forester at work does
[[nodiscard]] bool ReactsToWoodAgainAtOnce(entt::entity villager);

} // namespace openblack::ecs::villager_forester
