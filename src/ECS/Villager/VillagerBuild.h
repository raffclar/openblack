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

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/LivingAction.h"
#include "Enums.h"

// The builders: the way from 163 to a building site (CheckNeededForBuilding, SetupBuildingObject), the wood supply
// (SetupGetBuildingSupplies, DecideHowToGetWood, the storage pit 39), the ring of the site (GotoBuildingSite, 40, 184),
// the build cycle (41) and the builder book-keeping (EnterBuilding / ExitBuilding). The building side (sites, ring,
// pile, BuildBy) is ecs::building_sites / ecs::abodes; the villager reaches it through the Locator's
// villagerBuildingSites service. Positions are MapCoords x / z (ecs::town_queries).

namespace openblack::ecs::villager
{
// ---- the pure layer ----------------------------------------------------------------------------------------------

/// The build factor: f = 1 - a x 0.2, then below 1 -> 1, strictly above 1.2 -> 1.2. The game logic runs in float
/// precision (GUtilsDistance.h), so every step is a float step
[[nodiscard]] float BuildFactor(float alignment);
/// The wood used by one build cycle: u = trunc(WoodUsedPerBuildCycle x f) (a float product), and pile < u (unsigned) ->
/// u = pile
[[nodiscard]] uint32_t BuildAmount(float woodPerCycle, float factor, uint32_t pile);
/// The build progress of one cycle: u / the wood value, in float precision
[[nodiscard]] float BuildStep(uint32_t amount, float woodValue);
/// A valid ring index: 0 <= index < 128 (the ring's entries)
[[nodiscard]] bool BuildPosOk(int32_t index);
/// At the ring point when the distance in metres is <= 0.2
[[nodiscard]] bool NearRingPoint(float distance);
/// DecideHowToGetWood's store and forest weights. Builder mode (1): the store 1 when its stock is above the capacity
/// (compared unsigned), else 0; the forest 0.5. Mode 0: frac = 1 - (capacity + 1e-5) / (MaxWoodCarried + 1e-5)
/// (DropOffFraction), the store frac and the forest 1 - frac
struct WoodWeights
{
	float store {0.0f};
	float forest {0.0f};
};
[[nodiscard]] WoodWeights WoodSourceWeights(uint32_t stock, int16_t capacity, uint32_t maxWoodCarried, bool forBuilding);
/// DecideHowToGetWood's choice: store = GetDistanceModifier(d store, D) x storeW; forest = a forest ?
/// GetDistanceModifier(d forest, D) x forestW : 0; store strictly above forest -> 1; forest != 0 with a forest -> 2 (a
/// BigForest) / 3; else 0. `isBigForest` tells whether the forest has a BigForest
struct WoodChoice
{
	uint32_t how {0};
	float store {0.0f};
	float forest {0.0f};
};
[[nodiscard]] WoodChoice ChooseWoodSource(WoodWeights weights, float storeDistance, std::optional<float> forestDistance,
                                          bool isBigForest, float maxDistance);

// ---- the decision path -------------------------------------------------------------------------------------------

/// IsBuildingHappening; GetBestBuildingSite(me, disciple == BUILDER) -> SetupBuildingObject == 1 -> 1; else 0
uint32_t CheckNeededForBuilding(entt::entity villager);
/// The site's building, not built-and-repaired; CheckForClearArea (always 0) else SetupGetBuildingSupplies. Also used
/// for the worship site's building
uint32_t SetupBuildingObject(entt::entity villager, entt::entity site);
/// SetupBuildingObject for a building (from ArrivesHome, CheckInteractWithAbode, ...): the building's site
/// (GetBuildingSiteInList, else AddBuildingSite: a repair site) and SetupBuildingObject(site)
uint32_t SetupBuildingObjectForBuilding(entt::entity villager, entt::entity building);
/// The pushable objects in the building's clear area. Always 0 (no object type is pushable)
uint32_t CheckForClearArea(entt::entity villager, glm::ivec2 pos, float radius);

// ---- the wood supply ---------------------------------------------------------------------------------------------

/// ShouldIGetWood false -> GotoBuildingSite; else DecideHowToGetWood(1): 1 -> GotoStoragePitForBuildingMaterials; 2 ->
/// remember the site and walk to the BigForest (53), 1; 3 -> remember the site and VillagerGotoForest(forest, 49)
/// (VillagerForester.h)
uint32_t SetupGetBuildingSupplies(entt::entity villager, entt::entity site);
/// Where to get wood: 0 nothing, 1 the store (storage pit, home or the town's temporary wood pot, made if missing), 2 a
/// BigForest, 3 a forest
struct WoodSource
{
	uint32_t how {0};
	std::optional<uint32_t> forest;      ///< the forest (ecs::Trees id) of 3
	entt::entity bigForest {entt::null}; ///< the BigForest of 2
	float store {0.0f};                  ///< the scores (trace)
	float forestScore {0.0f};
};
WoodSource DecideHowToGetWood(entt::entity villager, bool forBuilding);
/// Full -> GotoBuildingSite; the pit's wood edge (or GetResourceDropoffPos(WOOD)); not a builder: no room and not a
/// BUILDER disciple -> 0, else remember the site unless the TOP's exit is ExitBuilding; the walk with FINAL 39
/// (footpath with a pit / home, else wall-hug)
uint32_t GotoStoragePitForBuildingMaterials(entt::entity villager, entt::entity site);

// ---- the site ----------------------------------------------------------------------------------------------------

/// SetTopState(163), remember the site, GetRandomBuildPos, the walk with FINAL 40 (footpath beyond 40 m)
uint32_t GotoBuildingSite(entt::entity villager, entt::entity site);

/// The entry of 39, 40, 41, 184: not a valid site -> 0 (refused); another entry function before -> AddBuilder; 1
uint32_t EnterBuilding(components::LivingAction& action, VillagerStates final, VillagerStates next);
/// The same exit -> 1; else RemoveBuilder (a valid site) and the site cleared; 1
uint32_t ExitBuilding(components::LivingAction& action, VillagerStates next);

/// State 39 ARRIVES_AT_STORAGE_PIT_FOR_BUILDING_MATERIALS (clip 340 P_PICK_UP_STICKS)
uint32_t ArrivesAtStoragePitForBuildingMaterials(components::LivingAction& action);
/// State 40 ARRIVES_AT_BUILDING_SITE (clip 348 P_PUT_DOWN_STICKS)
uint32_t ArrivesAtBuildingSite(components::LivingAction& action);
/// State 41 BUILDING (the building clip)
uint32_t BuildingState(components::LivingAction& action);
/// State 184 REENTER_BUILDING_STATE
uint32_t ReenterBuildingState(components::LivingAction& action);
} // namespace openblack::ecs::villager
