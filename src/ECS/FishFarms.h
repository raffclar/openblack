/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

// Fish farms: the fishermen, the food stock's takers and what the villager side calls. The shoal, its fish and the
// farm's process are in FishShoals.{h,cpp}. The fish puzzle's shoal-only FishFarm entities (FishPuzzle.cpp) have no
// town: TownFishFarms never gives them.

namespace openblack::ecs::fish_farms
{
/// A null villager -> nothing; else at the head, no duplicate test (EnterFishing tests first: HasFisherman), and the
/// villager's TargetThing = the farm (villager::SetTargetThing). No maximum (maxNoFishermanPerFishFarm is only in
/// Score)
void AddFisherman(entt::entity farm, entt::entity villager);
/// Every node of the villager out; TargetThing NOT written
void RemoveFisherman(entt::entity farm, entt::entity villager);
/// Is the villager in the list (EnterFishing's test)
[[nodiscard]] bool HasFisherman(entt::entity farm, entt::entity villager);
/// The number of fishermen (the fishing state's GameRand(n))
[[nodiscard]] uint32_t FishermanCount(entt::entity farm);
/// 1.0 x (1 - min((float)(fishermen / (int)maxNoFishermanPerFishFarm), 1)), truncated toward zero: 1 with no
/// fisherman, 0 with 1..4 (the truncation to int)
[[nodiscard]] int32_t Score(entt::entity farm);
/// Score's formula with the farm's fisherman count and the info's maxNoFishermanPerFishFarm
[[nodiscard]] int32_t ScoreFor(size_t fishermen, uint32_t maxFishermen);
/// r = Get2DRadius (5), h = r / 2; x + GameFloatRand(r) - h, then z + GameFloatRand(r) - h, each axis
/// (pos x 10 / 65536 + d) x 65536 / 10, truncated toward zero; the altitude copied
[[nodiscard]] map_coords::MapCoords FishingSpot(entt::entity farm);
/// FishingSpot's step after the two draws: the farm's position moved by dx and dz metres on each axis
[[nodiscard]] map_coords::MapCoords FishingSpotAt(map_coords::MapCoords pos, float dx, float dz);
/// The farm's position
[[nodiscard]] map_coords::MapCoords GetArrivePos(entt::entity farm);
/// (float)n <= food (or unordered) -> food -= n, n; else food truncated toward zero, and food = 0
int32_t RemoveFood(entt::entity farm, int32_t amount);
/// FOOD -> RemoveFood(n); any other type 0
int32_t RemoveResource(entt::entity farm, ResourceType type, int32_t amount);
/// FishFarm::town, entt::null without one
[[nodiscard]] entt::entity TownOf(entt::entity farm);
/// The farm's player: its town's owner; none without a town (the fish puzzle's farms)
[[nodiscard]] std::optional<PlayerNames> PlayerOf(entt::entity farm);
/// The town's fish farms, newest first (head insertion at construction): the FishFarm entities with that town, by
/// creation index from high to low. A null town gives none
[[nodiscard]] std::vector<entt::entity> TownFishFarms(entt::entity town);
/// "in the global fish farm list": a valid entity with a FishFarm
[[nodiscard]] bool IsFishFarm(entt::entity thing);
/// (openblack) disconnects the on_destroy<FishFarm> listener (fields::DisconnectDeletionListeners, before a Reset)
void DisconnectDeletionListener();
/// The farm's deletion (run by an on_destroy<FishFarm> listener that AddFisherman connects, so ecs::ToBeDeleted /
/// Registry::Destroy reach it): while the list is not empty, the head's SetTopState(163) (fields::ReleaseWorker)
/// relying on ExitFishing to unlink it; (openblack guard) a pass that does not shrink the list ends the loop (no hook
/// registered, or the villager side did not unlink it). The town list and the global list need nothing (the
/// components); RemoveMapObject is ecs::ToBeDeleted's generic part
void DeleteDependants(entt::entity farm);
} // namespace openblack::ecs::fish_farms
