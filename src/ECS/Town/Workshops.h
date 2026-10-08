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
#include <glm/mat3x3.hpp>

#include "3D/MapCoords.h"
#include "ECS/PotResource.h"
#include "Enums.h"

// The workshop and the town's workshop list.
// A workshop is an abode entity with components::Workshop; it turns 2500 wood into one scaffold (value 1) every 200
// town turns, at most 3 waiting at its slots (the mesh's special points 6, 7, 8). The scaffolds themselves are
// ecs::scaffolds' (ECS/Scaffolds.h). The villager side (supplying a workshop, the drop-off states 210 / 211) calls
// GetBestWorkshop and AddResource below.

namespace openblack::ecs::workshops
{
/// The wood one scaffold costs (WoodValue) is read from info.dat (2500). At most 3 scaffolds waiting: a literal
/// (the info field MaxNumMobileObjectsToProduce is not read)
constexpr int32_t k_MaxWaiting = 3;

// ---- lifetime ------------------------------------------------------------------------------------------------------

/// The entity has a components::Workshop
[[nodiscard]] bool IsWorkshop(entt::entity entity);
/// Sets up a workshop on an abode AbodeArchetype has just made (its Abode, Transform and Mesh in place): the fields
/// all 0, the ShowNeeds sign (one creation index, (not ported) the object), then CreatePileWood (the pile exists
/// during construction too). AbodeArchetype calls OnInsertedInMap below once the workshop is in its map cells
void Create(entt::entity workshop);
/// In the original the workshop enters the map cells before its pile, so the pile ends up at the head of each cell's
/// list. (openblack) Create already made the pile, to keep the creation index order: it is taken out of its cells and
/// put back here, after AbodeArchetype's InsertMapObject of the workshop
void OnInsertedInMap(entt::entity workshop);
/// When there is no pile yet, a "Magic Wood" pot at the special point 4 (GetResourcePosAndYAngle) with 0 wood
void CreatePileWood(entt::entity workshop);
/// WOOD with a 3D object whose special point 4 exists -> that point (and its Y angle); else the workshop's own
/// position and angle 0. `angle` may be null (openblack guard)
[[nodiscard]] map_coords::MapCoords GetResourcePosAndYAngle(entt::entity workshop, ResourceType type, float* angle);
/// The wood pile (null when none)
[[nodiscard]] entt::entity GetPileWood(entt::entity workshop);
/// The workshop whose wood pile this is, or null. (openblack) the pile keeps no link of its own: the workshops are
/// searched
[[nodiscard]] entt::entity WorkshopOfPile(entt::entity pile);
/// The workshop part of making it functional, after the abode's part (abodes::MakeFunctional runs it first): with a
/// town, the pulse, then AddWorkshop
void MakeFunctional(entt::entity workshop);
/// With a town RemoveWorkshop; then every owned scaffold, head first, loses its owner (when available) and all its
/// nodes go (no owned count / slot update, literal). The abode's own part is abodes::DestroyedByEffect's
void DeleteDependants(entt::entity workshop);
/// DeleteDependants; the pile is unlinked and deleted (its wood is lost); the ShowNeeds sign is deleted; `now` goes
/// to both. The abode's own deletion is the caller's
void ToBeDeleted(entt::entity workshop, bool now = false);
/// The pile -> DoResourceRemoving(its type, its amount) off the mirror, and the pile link is cleared. (pending) the
/// structure's own part is not read
void RemovePotFromStructure(entt::entity workshop, entt::entity pot);

// ---- the town list -------------------------------------------------------------------------------------------------

/// When not already in the list, at the head
void AddWorkshop(entt::entity town, entt::entity workshop);
/// Out of the list only when found (callers DeleteDependants and the structure's removal from its town)
void RemoveWorkshop(entt::entity town, entt::entity workshop);
/// best = 0; each workshop head first: v = GetDistanceModifier(GetDistanceInMetres(pos, its pos), 500) x
/// (useDesire ? GetDesireToBeSupplied : 1); strictly above best (NaN skipped) and (!onlyFunctional || IsFunctional) ->
/// kept. Null when none. The villager asks with (pos, 1, 1) when supplying a workshop, at the storage pit for workshop
/// materials (state 211; a wood capacity of 0 goes to 163) and on arrival for the drop-off (state 210: it asks
/// again); without a town 210 and 211 return 0 before asking
[[nodiscard]] entt::entity GetBestWorkshop(entt::entity town, const map_coords::MapCoords& pos, bool useDesire,
                                           bool onlyFunctional);
/// No workshop of the town has IsPosWithinScaffoldAreas(scaffold pos) (callers: the scaffold in the hand, choosing a
/// plan, building a planned building)
[[nodiscard]] bool IsScaffoldAwayFromWorkshops(entt::entity town, entt::entity scaffold);
/// CheckSnapToPoint of every workshop of the town (all are tried, the results ORed). Called when a scaffold's physics
/// ends
bool CheckScaffoldSnapToPoint(entt::entity town, entt::entity scaffold);

// ---- the turn ----------------------------------------------------------------------------------------------------

/// The workshop's turn (step 4 of the town's turn): the scaffold production when functional, the head scaffold's
/// release once it can no longer be adjusted, the ShowNeeds desire, then the abode's turn (the site's Process and
/// abode_villagers::ProcessAbode) at its end
void Process(entt::entity workshop);
/// 3 - (a scaffold in production ? 1 : 0) - owned scaffolds (signed)
[[nodiscard]] int32_t GetSpaceInStore(entt::entity workshop);
/// 1 - min((wood + 0.0001) / (WoodValue + 0.0001), 1), in float
[[nodiscard]] float GetVisualWoodDesire(entt::entity workshop);
/// space != 0 || GetVisualWoodDesire > 0 ? 1 : 0
[[nodiscard]] float GetDesireToBeSupplied(entt::entity workshop);
/// For the hand-over tooltip: space > 0 ? WoodValue x space - wood (uint32, wrapping, literal) : 0
[[nodiscard]] uint32_t WoodWanted(entt::entity workshop);

// ---- resources ---------------------------------------------------------------------------------------------------

/// n == 0 -> 0. The pulse when the workshop had none of the type. WOOD: no pile -> CreatePileWood; an available pile
/// -> its AddToPotDirect. Then DoResourceAdding(type, added) with no status (no desire / alignment / belief part)
/// onto the mirror. Returns what was added (0 for any other type: only the mirror's DoResourceAdding with 0).
/// Callers: object_resources::AddResource on a workshop or its pile, the workshop's building site, the villager's
/// drop-off (the amount it carries)
uint32_t AddResource(entt::entity workshop, ResourceType type, uint32_t amount, bool poisoned = false);
/// WOOD with a pile -> r = its JustRemoveResource, then DoResourceRemoving(WOOD, r, status) (the mirror); any other
/// case DoResourceRemoving(type, n, status). No building-site redirect (the workshop overrides the abode's).
/// object_resources::RemoveResource routes a workshop here
uint32_t RemoveResource(entt::entity workshop, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper = {});
/// Removing from the workshop's pile (a pile part of a structure that is not a storage pit): the amount over the
/// maximum is 0 for a workshop; r = the pile's JustRemoveResource(n) (it stays when emptied: part of a structure);
/// r != 0 -> the workshop's DoResourceRemoving(type, r, status); r < n -> r += the workshop's RemoveResource(n - r).
/// For ObjectResources
uint32_t RemoveResourceFromPile(entt::entity pile, ResourceType type, uint32_t amount,
                                const pot_resource::Dropper& dropper = {});
/// (WOOD with a site) || WOOD || ANY (-2)
[[nodiscard]] bool IsResourceStore(entt::entity workshop, ResourceType type);

// ---- slots and scaffolds -----------------------------------------------------------------------------------------

/// The special point 6 + slot (0 -> 6, 1 -> 7, 2 -> 8, any other: the argument itself, literal); x, z kept, the
/// altitude 0 (on the land). `yAngle` gets the point's Y angle (null allowed)
[[nodiscard]] map_coords::MapCoords GetSlotPos(entt::entity workshop, int32_t slot, float* yAngle);
/// The first free slot, else 0 (literal: slot 0 even when it is taken)
[[nodiscard]] int32_t GetFirstFreeSlot(entt::entity workshop);
/// GetDistanceInMetres(p, special point n) < r (strict; no point: (0, 0, 0))
[[nodiscard]] bool IsNearSpecialPoint(entt::entity workshop, int32_t point, const map_coords::MapCoords& pos, float radius);
/// dist(own pos, p) <= 50 (NaN passes too) and (the drop area IsNearSpecialPoint(10, p, 10) | IsNearSpecialPoint(6 / 7 / 8, p,
/// 5)), all four asked
[[nodiscard]] bool IsPosWithinScaffoldAreas(entt::entity workshop, const map_coords::MapCoords& pos);
/// s within 2 m of the special point 6, 7 or 8 (all three asked): another owner's -> nothing when GetSpaceInStore ==
/// 0, else s leaves its workshop, AddScaffold(s), s owner = this, s slot = the first free one; then the snap (moved to
/// its slot's special point, slot occupied). True when snapped
bool CheckSnapToPoint(entt::entity workshop, entt::entity scaffold);
/// The rotation a snapped scaffold takes from its slot's special point (the workshop's snap): the point's
/// DecomposeYXZ angles through SetXYZAngles, so its scale and skew are dropped; no point: no rotation
[[nodiscard]] glm::mat3 SnappedRotation(const std::optional<glm::mat3>& pointRotation);
/// s -> a node at the head of the list (none for a null s); the owned count goes up in any case
void AddScaffold(entt::entity workshop, entt::entity scaffold);
/// Only caller scaffolds::RemoveFromWorkshop. s not in the list -> nothing; every node of s out; owned count down;
/// its slot freed; 2 owned now and a town -> the pulse
void RemoveScaffold(entt::entity workshop, entt::entity scaffold);
/// s is in the workshop's scaffold list (scaffolds::RemoveFromWorkshop asks it)
[[nodiscard]] bool OwnsScaffold(entt::entity workshop, entt::entity scaffold);
/// slot < 3 -> slot freed
void FreeSlot(entt::entity workshop, int32_t slot);
/// Called when the scaffold is taken into the hand or starts its physics: its slot becomes k_SlotTakenAway (the place
/// stays reserved). The scaffold's slot is read and written through scaffolds::GetSlot / SetSlot (ECS/Scaffolds.h),
/// its owner through scaffolds::GetOwner / SetOwner
void ScaffoldMoved(entt::entity workshop, entt::entity scaffold);
/// The slot's state: k_SlotFree / k_SlotTakenAway / k_SlotOccupied; 0 outside 0..2 or without a workshop. Read when
/// asking whether a scaffold can become a physics object (2: occupied)
[[nodiscard]] uint8_t GetSlotState(entt::entity workshop, int32_t slot);
} // namespace openblack::ecs::workshops
