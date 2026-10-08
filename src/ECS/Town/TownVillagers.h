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
#include <glm/vec2.hpp>

// The town's villagers: the homeless list (Town::homelessVillagers, the head first), the global vagrants,
// AddVillagerToTown, FindAbodeWithSpaceInTown, the eaten food and ShuffleVillagersAroundAbodes (step 24 of the town's
// turn). The abode's side is ecs::abode_villagers.

namespace openblack::ecs::components
{
struct Town;
}

namespace openblack::ecs::town_villagers
{
// ---- the homeless and the vagrants -------------------------------------------------------------------------------

/// The town's homeless, the head first (MakeHomelessNoStateChange inserts at the head). Empty without a town
[[nodiscard]] const std::vector<entt::entity>& Homeless(entt::entity town);
/// The villager is in the town's homeless list
[[nodiscard]] bool IsVillagerInHomelessList(entt::entity town, entt::entity villager);
/// At the head of the homeless list (MakeHomelessNoStateChange)
void AddToHomelessList(entt::entity town, entt::entity villager);
/// The unlink every writer does (adding to an abode, a homeless villager moving into an abode, RemoveVillager): out of
/// the list if it is there. Returns whether it was there
bool RemoveFromHomelessList(entt::entity town, entt::entity villager);
/// The villagers without a town. Its writers are the map script's CREATE_VILLAGER / _POS without an abode or a town,
/// a child's birth, a town's deletion and the release from a script
[[nodiscard]] const std::vector<entt::entity>& Vagrants();
/// The villager is in the vagrants list
[[nodiscard]] bool IsVagrant(entt::entity villager);
/// The unlink when added to an abode, made homeless or deleted
bool RemoveFromVagrants(entt::entity villager);
/// At the head of the vagrants list, unless it is in the list already (the release from a script). True when added
bool AddToVagrants(entt::entity villager);
/// A new map (Script::BeginMapFeatures, (inferred): the game starts with none, and each deleted villager is unlinked)
/// / the tests: no vagrants
void ClearVagrants();

// ---- joining, moving, eating -------------------------------------------------------------------------------------

/// Uninhabitable -> 0; the town's counts; SetTown; an abode of this town -> done; another one ->
/// RemoveAliveVillagerFromAbode, SetAbode(0); FindAbodeWithSpaceInTown(v, 0) -> AddVillagerToAbode, 1; none ->
/// MakeHomelessNoStateChange; then adults + children == 1 -> worship::town::CheckAddWorshipSite. 1
bool AddVillagerToTown(entt::entity town, entt::entity villager);
/// The town's structures (newest first), IsFunctional, the strictly best CalculateScoreForAddingVillagerToAbode above
/// `minimum`; entt::null when none
[[nodiscard]] entt::entity FindAbodeWithSpaceInTown(entt::entity town, entt::entity villager, float minimum);
/// The town's counts: with an abode its places move from child to adult; children - 1, adults + 1 (the places are
/// recomputed each town turn; the adults / children are kept at once)
void ChildToAdult(entt::entity town, entt::entity villager);
/// Town::foodUsed += n; the player's statistics. TODO(statistics)
void UseFood(entt::entity town, uint32_t amount);
/// FindChildrenAndOrphanThem, the town's counts, an abode -> RemoveAliveVillager and SetAbode(0), else out of the
/// homeless list; out of the town's on-the-way-to-worship list; SetTown(0); the empty town's countdown = 50; mother =
/// 0. TODO(miracles): RemoveVillagerFromWorshipSite (private to VillagerWorship.cpp; the worship states' exits do it)
void RemoveVillager(entt::entity town, entt::entity villager);

// ---- the shuffle (step 24 of the town's turn) ---------------------------------------------------------------------

/// (town id x 20 + turn, truncated toward zero) % shuffleVillagersEvery (unsigned) == 0
[[nodiscard]] bool ShuffleDue(const components::Town& town, uint32_t turn);
/// ShuffleDue's formula with the town's id and the info's shuffleVillagersEvery (0: never)
[[nodiscard]] bool ShuffleDueEvery(uint32_t townId, uint32_t turn, uint32_t every);
/// Moves villagers between two of the town's abodes to even out their desires for males and villagers (one move a
/// call)
void ShuffleVillagersAroundAbodes(entt::entity town);

/// One entry of the shuffle's list (the abode, CalculateDesireToGainMale, 0.5 x CalculateDesireToGainVillager)
struct ShuffleEntry
{
	entt::entity abode {entt::null};
	float male {0.0f};
	float villager {0.0f};
};
/// |b.m^2 + b.v^2| < |a.m^2 + a.v^2| -> -1, else 1 (never 0: the larger first)
[[nodiscard]] int ShuffleCompare(const ShuffleEntry& a, const ShuffleEntry& b);
/// The original's qsort (as town_desire::MsvcQsort) of the list with ShuffleCompare
void SortShuffle(std::vector<ShuffleEntry>& entries);
/// The pair loop on a sorted list, without the moves: which pair it tries first and how. For the
/// tests (`swap` = SwapMaleForFemaleFrom, else TakeVillagerFrom; `first` the one whose function runs; `male` its arg)
struct ShufflePlan
{
	size_t a {0};
	size_t b {0};
	bool swap {false};
	bool firstIsA {true};
	bool male {false};
};
/// The plan for entry i of a sorted list: the j > i whose |(a + c)^2| is strictly the smallest below |a^2|; none ->
/// false. `percentAdults(abode)` is GetPercentAbodeFullWithAdults (the signs-opposite rule)
[[nodiscard]] bool PlanShuffle(const std::vector<ShuffleEntry>& sorted, size_t i, float (*percentAdults)(entt::entity),
                               ShufflePlan& plan);

// ---- towns near a point ------------------------------------------------------------------------------------------

/// map_cells::GetNearestTown: every player's towns, GetDistanceInMetres < best, best = r (strict)
[[nodiscard]] entt::entity GetNearestTown(glm::ivec2 pos, float radius);
} // namespace openblack::ecs::town_villagers
