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
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Town/TownStores.h"
#include "Enums.h"

namespace openblack
{
struct GVillagerInfo;
}

// What a villager carries, takes from and leaves in the structures. The food half: eating from home and from the
// storage pit; the carrying half: the pick-ups and drops, the capacities, the carried object, the drop-off
// states 31 / 32, the temporary pots (ecs::town_stores) and the load's speed factors. Positions are MapCoords x / z
// (ecs::town_queries). The pure layer has the arithmetic for the tests.

namespace openblack::ecs::villager
{
// ---- the pure layer ----------------------------------------------------------------------------------------------

/// The larger load: food > wood (unsigned 16-bit) -> FOOD and the food; else wood != 0 -> WOOD and the wood; else
/// None and 0 (so equal non-zero loads give WOOD)
struct HeldResource
{
	ResourceType type {ResourceType::None};
	uint16_t amount {0};
};
[[nodiscard]] HeldResource HeldLarger(int16_t food, int16_t wood);
/// The carried log for the tree type in flags bits 14-15: 1 -> 13 TREE_1, 2 -> 14 TREE_2, 3 -> 15 TREE_3, 0 -> 12
/// WOOD
[[nodiscard]] int32_t WoodCarriedObject(uint16_t flags);
/// The state's exit handler is the building exit (k_OriginalStateFns): true for a builder's final state
[[nodiscard]] bool IsBuildingExitState(VillagerStates state);
/// What CarriedObjectFor reads
struct CarriedInput
{
	VillagerStates finalState {VillagerStates::InvalidState}; ///< GetFinalState
	VillagerStates topState {VillagerStates::InvalidState};   ///< The top state
	float life {1.0f};                                        ///< The villager's life
	int16_t wood {0};                                         ///< The held wood
	int16_t food {0};                                         ///< The held food
	uint16_t flags {0};                                       ///< The tree type, bits 14-15
	bool finalIsBuilding {false};                             ///< IsBuildingExitState of the final state
	int32_t rowCarriedFinal {0};                              ///< The final state's row carried object, 0 = none
	int32_t rowCarriedTop {0};                                ///< The top state's row carried object
	int32_t previous {1};                                     ///< The carried object before the call
	float lifeWhenCrawlsWounded {0.15f};                      ///< From the villager info
	uint32_t minWoodToShowGraphic {50};                       ///< From the villager info
	uint32_t minFoodToShowGraphic {100};                      ///< From the villager info
};
/// The new carried object (CARRIED_OBJECT) for the villager's state and load
[[nodiscard]] int32_t CarriedObjectFor(const CarriedInput& in);
/// The load's speed factors, clamp(1 + SpeedModWhenFullLoad - held / Max, 0.75, 1), at float precision
struct LoadFactor
{
	float wood {1.0f};
	float food {1.0f};
};
[[nodiscard]] LoadFactor LoadFactors(int16_t wood, int16_t food, const GVillagerInfo& info, bool trader);
/// The town needs' speed factor: base + clamp(S / divisor, 0, 0.5), S = town_desire::TownNeedsSum (float precision)
[[nodiscard]] float TownNeedsFactor(float townNeedsSum, const GVillagerInfo& info);
/// The drop-off score: frac = (float)(1 - (capacity + 1e-5) / (MaxFoodCarried + 1e-5)); GetDistanceModifier(distance,
/// 500) x frac (float precision). `held` is the held food (capacity = (int16)(max - held))
[[nodiscard]] float DropOffFraction(int16_t capacity, uint32_t maxFoodCarried);
[[nodiscard]] float DropOffScore(int16_t held, uint32_t maxFoodCarried, float distance);
/// Whether a log is dropped and its wood multiplier ((float)(wood / the log's wood value)); nullopt when nothing is
/// dropped (carried object <= 1 or >= 16, or wood <= MinWoodToShowGraphic)
struct DroppedLog
{
	int32_t carriedObject {1}; ///< The carried object: its mesh is CarriedProps k_PropMeshes[it]
	float multiplier {0.0f};
};
[[nodiscard]] std::optional<DroppedLog> DroppedLogFor(int32_t carriedObject, int16_t wood, uint32_t minWoodToShowGraphic,
                                                      float logWoodValue);
/// A dropped log's wood: truncated woodValue x multiplier x scale (float steps)
[[nodiscard]] int32_t DroppedLogValue(uint32_t woodValue, float multiplier, float scale);

/// The villager's walking speed in metres, openblack's WallHug::speed (0 without one)
[[nodiscard]] float SpeedInMetres(entt::entity villager);

// ---- carrying ----------------------------------------------------------------------------------------------------

/// Picks up n of a type: FOOD adds to the held food (a 16-bit add) and the town's carried food; any other type (also
/// -2) adds to the held wood and the town's carried wood and sets the tree type bits to tree & 3. Returns n
int16_t PickupResource(entt::entity villager, ResourceType type, int16_t amount, uint8_t treeType);
/// PickupResource(FOOD, n, 0)
void PickupFood(entt::entity villager, int16_t amount);
/// PickupResource(WOOD, n, tree)
void PickupWood(entt::entity villager, int16_t amount, uint8_t treeType);
/// Drops n food: n == 0 or n above what it carries (unsigned 16-bit compare) -> all of it; the held food and the
/// town's carried food go down by n. The food is gone (no pot is made). Returns n
uint16_t DropFood(entt::entity villager, uint16_t amount);
/// DropFood for the wood. The tree bits stay (literal). Returns n
uint16_t DropWood(entt::entity villager, uint16_t amount);
/// WOOD -> DropWood, FOOD -> DropFood, else 0
uint16_t DropResource(entt::entity villager, ResourceType type, uint16_t amount);
/// (int16)(info.MaxFoodCarried / MaxWoodCarried - held); negative above the maximum
[[nodiscard]] int16_t GetFoodCapacity(entt::entity villager);
[[nodiscard]] int16_t GetWoodCapacity(entt::entity villager);
/// HeldLarger of the held food and wood
[[nodiscard]] uint16_t GetResourceHeld(entt::entity villager, ResourceType& type);
/// A villager as the target of object_resources::AddResource: FOOD -> PickupFood(n) and, poisoned,
/// ecs::life::TakePoisonedResource; WOOD -> PickupWood(n, 0); other types nothing. Returns 0
/// always (literal)
uint32_t AddResourceToVillager(entt::entity villager, ResourceType type, uint32_t amount, bool poisoned);
/// Takes n from an object: c = object_resources::RemoveResource(type, n); c != 0 -> PickupResource(type, c, 0) (an
/// object's carried tree type is 0); abodes and pits never give a food speed-up; a poisoned object ->
/// ecs::life::TakePoisonedResource. Returns c
uint16_t GetResourceFrom(entt::entity villager, entt::entity object, ResourceType type, int16_t amount);

// ---- the carried object ------------------------------------------------------------------------------------------

/// WoodCarriedObject of the villager's flags
[[nodiscard]] int32_t GetWoodCarriedObject(entt::entity villager);
/// The final state's exit is the building exit
[[nodiscard]] bool FinalStateIsBuilding(entt::entity villager);
/// The carried object CARRIED_OBJECT it would set when its animation is chosen, from `previous` (the current one,
/// kept with the final state 4 IN_SCRIPT or TOP 200 SCRIPT_PLAY_ANIM). The caller stores it
/// (SkeletalAnimation::carriedObject)
[[nodiscard]] int32_t SetStateCarriedObject(entt::entity villager, int32_t previous);

// ---- where: the storage pit, the home and the temporary pot ------------------------------------------------------

/// The town's storage pit or, without one, the villager's abode
[[nodiscard]] entt::entity GetStoragePit(entt::entity villager);
/// Where to drop a type: GetStoragePit functional -> its GetArrivePos; else the town's storage pit functional -> its
/// GetArrivePos; else with a town the point of the town's temporary pot of that type (made if it has none: a side
/// effect, literal); without a town the villager's position
[[nodiscard]] glm::ivec2 GetResourceDropoffPos(entt::entity villager, ResourceType type);
/// The town's temporary store of a type through the villagerStores service (the pot is made if the town has none).
/// For the builder's wood search (VillagerBuild.cpp)
[[nodiscard]] town_stores::TemporaryStore GetTemporaryStore(entt::entity town, const map_coords::MapCoords& from,
                                                            ResourceType type);
/// Where a villager takes from or gives to an object: a storage pit's GetArrivePos, else the object's position
[[nodiscard]] glm::ivec2 GetResourceNearestEdge(entt::entity object, ResourceType type, entt::entity villager);
/// Takes n from a structure: pos = GetResourceNearestEdge; within my 2D radius -> c = GetResourceFrom(object, type,
/// n): 0 -> 0, c < n -> 0x24, else 1; not there -> SetupMoveToWithHug(pos, GetFinalState), 0x24
uint32_t AtStructureRemoveResource(entt::entity villager, entt::entity object, ResourceType type, uint32_t amount);
/// Gives n to a structure: edge = GetResourceNearestEdge; within my speed -> added =
/// object_resources::AddResource(type, n): 0 -> 0, else DropResource(type, added), n = added, 1; not there ->
/// SetupMoveToWithHug(edge, GetFinalState), n = 0, 0x24
uint32_t AtStructureAddResource(entt::entity villager, entt::entity object, ResourceType type, uint32_t& amount);
/// Takes n of a type from the storage pit (or the town's temporary pot), then goes to `ok`, or to `fail`
uint32_t ArrivesAtStoragePitForResource(entt::entity villager, ResourceType type, uint32_t amount, VillagerStates ok,
                                        VillagerStates fail);

// ---- dropping off (states 31 / 32) -------------------------------------------------------------------------------

/// Also called as a function by the food desire check: the pit (or home) functional ->
/// SetupMoveToOnFootpath(pit, arrive, 32), 1; holding nothing -> SetTopState(163), 0; else
/// SetupMoveToWithHug(GetResourceDropoffPos(type), 32), 1
uint32_t GotoStoragePitForDropOff(entt::entity villager);
/// 31 GOTO_STORAGE_PIT_FOR_DROP_OFF (the state table's adapter of GotoStoragePitForDropOff)
uint32_t GotoStoragePitForDropOffState(components::LivingAction& action);
/// 32 ARRIVES_AT_STORAGE_PIT_FOR_DROP_OFF (clip 347 P_PUT_DOWN_BAG)
uint32_t ArrivesAtStoragePitForDropOff(components::LivingAction& action);

// ---- dropped resources -------------------------------------------------------------------------------------------

/// The carried wood falls as a dead tree log (a pine, the carried object's mesh, the multiplier wood / its wood
/// value) put into physics with the velocity a (none: 0, and b ignored), the angular b and the extra vector c; then
/// DropWood(0). TODO(trees/physics): the log is not made (no dead tree creation / physics API yet): (approximate)
/// the wood is lost
void CreateDroppedResource(entt::entity villager, std::optional<glm::vec3> velocity, std::optional<glm::vec3> angular,
                           std::optional<glm::vec3> extra);
} // namespace openblack::ecs::villager
