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
#include <glm/vec2.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
struct Villager;
} // namespace openblack::ecs::components

/// A villager building for its town: it takes up the site its town most wants built, fetches wood from the storage pit
/// when the site lacks it, carries it to one of the places round the building, puts it down there and builds in strokes,
/// each using some of the site's wood to raise the building, until the building stands or the wood runs out. Also the
/// crowd a script gathers villagers into. The state functions are the living action system's.
namespace openblack::ecs::villager_build
{

/// Whether a state is one of the building states, which share their builder's place at its site
[[nodiscard]] bool IsBuildingState(VillagerStates state);

/// The town wants building: the villager takes up its best site. 1 when it did.
uint32_t CheckSatisfyToBuild(components::LivingAction& action);
/// The town wants a home or a civic building: with any site it takes up the best one. 1 when it did.
uint32_t CheckNeededForBuilding(components::LivingAction& action);
/// The town wants its buildings mended: the villager takes up the town's mending site that wants builders most
uint32_t CheckSatisfyToRepair(components::LivingAction& action);

uint32_t ArrivesAtStoragePitForBuildingMaterials(components::LivingAction& action);
uint32_t ArrivesAtBuildingSite(components::LivingAction& action);
uint32_t Building(components::LivingAction& action);
uint32_t ReenterBuildingState(components::LivingAction& action);
/// Going into a building state from another kind of state it counts once more among the site's builders, and leaving
/// for another kind it is let go of the site
bool EnterBuilding(components::LivingAction& action, VillagerStates previous, VillagerStates next);
bool ExitBuilding(components::LivingAction& action, VillagerStates next);

/// A villager carrying enough wood or food to show takes it to the storage pit. 1 when it went.
uint32_t CheckTakeResourcesToStoragePit(components::LivingAction& action);
uint32_t GotoStoragePitForDropOff(components::LivingAction& action);
uint32_t ArrivesAtStoragePitForDropOff(components::LivingAction& action);
/// The same at a given storage pit, which may be another town's: the load still counts off its own town's carried
uint32_t DropOffAtStoragePit(components::LivingAction& action, std::optional<entt::entity> pit);

/// At its town's storage pit the villager takes what it can of up to an amount of a resource, walking to the pit
/// first, then walks off into a state; with none to take, it goes into another state
uint32_t ArrivesAtStoragePitForResource(components::LivingAction& action, ResourceType type, uint32_t amount,
                                        VillagerStates arrive, VillagerStates fail);

/// Turns the villager towards a point by at most a step of the game's angles; whether it now faces it
bool FaceTowards(entt::entity villager, glm::vec2 point, int32_t step);
/// Whether the villager is a builder disciple, offered sites even when no builder is wanted
[[nodiscard]] bool IsBuilderDisciple(const components::Villager& villager);
/// The villager walks to a place round a site's building to build there, if it works there already or the site wants
/// builders. 1 when it went.
uint32_t GotoBuildingSite(components::LivingAction& action, entt::entity building);

/// The villager takes up building a site, fetching wood first when it lacks it. 1 when it did.
uint32_t TakeUpSite(components::LivingAction& action, entt::entity building);

/// In a script's crowd: a fresh crowd clip each time the last has played
uint32_t ScriptInCrowd(components::LivingAction& action);

} // namespace openblack::ecs::villager_build
