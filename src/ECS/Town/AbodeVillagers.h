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

#include <vector>

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack
{
struct GAbodeInfo;
}

// The abode's villagers: the list (Abode::inhabitants, the head first), the pair (Abode::maleFemale), the counts,
// PresentAtHome, the score of a villager for the abode, the abode's turn (TownProcess step 4) and the two moves of the
// town's shuffle (step 24). The town's side (the homeless list, AddVillagerToTown, FindAbodeWithSpaceInTown) is
// ecs::town_villagers.

namespace openblack::ecs::abode_villagers
{
// ---- reading -----------------------------------------------------------------------------------------------------

/// The abode's villagers, the head first (AddVillagerToAbode inserts at the head). An empty list for anything that is
/// not an abode
[[nodiscard]] const std::vector<entt::entity>& VillagersOf(entt::entity abode);
/// The villagers inside now (only ArriveHome / LeaveHome change it)
[[nodiscard]] uint8_t PresentAtHome(entt::entity abode);
/// The abode's GAbodeInfo: town_stats::AbodeInfoOf with its town's tribe; nullptr when none
[[nodiscard]] const GAbodeInfo* InfoOf(entt::entity abode);
/// The town of Abode::townId; entt::null without one
[[nodiscard]] entt::entity TownOf(entt::entity abode);
/// GAbodeInfo's MaxVillagersInAbode / MaxChildrenInAbode (0 without an info)
[[nodiscard]] uint32_t MaxVillagers(entt::entity abode);
[[nodiscard]] uint32_t MaxChildren(entt::entity abode);
/// max - count, signed
[[nodiscard]] int32_t GetRoomLeftForAdults(entt::entity abode);
[[nodiscard]] int32_t GetRoomLeftForChildren(entt::entity abode);
/// MaxVillagers 0 -> 1; else AdultCount / MaxVillagers >= percentTooCrowded
[[nodiscard]] bool IsTooCrowded(entt::entity abode);
/// AdultCount / MaxVillagers; MaxVillagers 0 -> 1
[[nodiscard]] float GetPercentAbodeFullWithAdults(entt::entity abode);
/// ChildCount / MaxChildren as an INTEGER division (0 or 1); MaxChildren 0 -> 1
[[nodiscard]] float GetPercentAbodeFullWithChildren(entt::entity abode);
/// ScoreForAdding with the abode's counts and the distance
[[nodiscard]] float CalculateScoreForAddingVillagerToAbode(entt::entity abode, entt::entity villager);
/// DesireToGainMale with the town's men and women
[[nodiscard]] float CalculateDesireToGainMale(entt::entity abode);
/// DesireToGainVillager with the town's adults and adult places
[[nodiscard]] float CalculateDesireToGainVillager(entt::entity abode);

// ---- the list ----------------------------------------------------------------------------------------------------

/// Out of the town's homeless list, out of its old abode, or out of the vagrants;
/// at the head of the list; SetAbode (its town = the abode's); AddVillagerToTown if that town is not the villager's;
/// a child ++ChildCount, an adult MaleFemale[sex] (if empty), ++AdultCount, AdultMaleCount += IsMale
void AddVillagerToAbode(entt::entity abode, entt::entity villager);
/// Inside -> SetTopState(163) (its exit does the LeaveHome); the counts
/// (not below 0); out of the list; SetAbode(0). MaleFemale is not touched (literal)
void RemoveAliveVillagerFromAbode(entt::entity abode, entt::entity villager);
/// The villager is being deleted: MaleFemale[sex] == it -> BOTH to none; the counts; the list; SetAbode(none);
/// town_villagers::RemoveVillager
void RemoveDeletedVillagerFromAbode(entt::entity abode, entt::entity villager);
/// The abode is destroyed: HomeDeleted for each villager
void RemoveAllVillagersFromAbode(entt::entity abode);
/// PresentAtHome + 1 / - 1, wrapping like a byte
void ArriveHome(entt::entity abode);
void LeaveHome(entt::entity abode);
/// --ChildCount (not below 0), ++AdultCount, AdultMaleCount += IsMale, and the town's ChildToAdult
void ChildToAdult(entt::entity abode, entt::entity villager);
/// On x: the first man of y's list and the first woman of x's that are not
/// inside: ForceMoveVillagerToAbode(man -> x), (woman -> y); 1. No such pair: 0
uint32_t SwapMaleForFemaleFrom(entt::entity x, entt::entity y);
/// On x: the first of y's list of that sex that is not inside -> ForceMoveVillagerToAbode(-> x); 1. None: 0
uint32_t TakeVillagerFrom(entt::entity x, entt::entity y, bool male);

// ---- the turn ----------------------------------------------------------------------------------------------------

/// The abode's turn (TownProcess step 4, for the abodes whose class does not override it)
void ProcessAbode(entt::entity abode);
/// Whether the abode's class runs ProcessAbode as its turn (Abode, StoragePit, Creche, Wonder, Graveyard); Field,
/// TownCentre, Workshop and SpellDispenser have their own
[[nodiscard]] bool RunsAbodeProcess(entt::entity abode);

// ---- the pure layer (tests) --------------------------------------------------------------------------------------

/// f = count / max, min(f, 1); room = 1 - f, room <= 0 -> room; sex = listSize == 0 ? 1 :
/// ((1 - sameSex / listSize) + 1) x 0.5; (GetDistanceModifier(distance, 500) + 1) x 0.5 x sex x room; max 0 -> 0
[[nodiscard]] float ScoreForAdding(uint32_t count, uint32_t max, float sameSex, uint32_t listSize, float distance);
/// (males + 0.001) / (females + 0.001) - (adultMales + 0.001) / ((adults - adultMales) + 0.001)
[[nodiscard]] float DesireToGainMale(uint32_t townMales, uint32_t townFemales, uint8_t adults, uint8_t adultMales);
/// (townAdults + 0.001) / (townAdultPlaces + 0.001) - percentAdults
[[nodiscard]] float DesireToGainVillager(uint32_t townAdults, uint32_t townAdultPlaces, float percentAdults);
} // namespace openblack::ecs::abode_villagers
