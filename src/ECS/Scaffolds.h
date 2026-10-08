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
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ECS/PotResource.h"
#include "Enums.h"

// The scaffold: a mobile object (components::Scaffold) that stands for 1..n scaffolds (its scale is its value), offers
// the building the nearest town wants (a phantom while it is held) and, put down gently, becomes that building's
// site. Made by the workshops, by CHL CREATE and by the hand (the surplus, the tap split). Villagers never carry
// scaffolds (the "scaffolds on the way" lists and states are unused in the original). The hand calls the "hand" block,
// the physics the "physics" block; the workshop side is ecs::workshops (ECS/Town/Workshops.h).

namespace openblack::ecs::physics
{
struct PhysicsObject;
}

namespace openblack::ecs::scaffolds
{
// ---- the object -----------------------------------------------------------------------------------------------------

/// A new scaffold at pos for the town, with the owner workshop (or null), its Y angle and scale (its value). Both
/// plans start empty at pos. The info is always the shared scaffold info (info.scaffold). Registers the class's tap
/// and physics handlers once
entt::entity Create(const map_coords::MapCoords& pos, entt::entity town, entt::entity owner, float yAngle, float scale);
/// Whether the entity has components::Scaffold
[[nodiscard]] bool IsScaffold(entt::entity e);
/// (GetScale() - 0.5f) * 5.0f + 0.5f, truncated toward zero: the scaffolds it stands for
[[nodiscard]] uint32_t GetValue(entt::entity s);
/// SetScale(float(n) * 0.2f + 0.5f)
void SetValue(entt::entity s, uint32_t n);
/// Value x info WoodValue (2500): the object's resource for WOOD
[[nodiscard]] uint32_t GetDefaultResource(entt::entity s);
/// Adjustable unless placed at least gameTurnsAfterPlacingCanStillPickUp (150) turns ago and free to offer any building
[[nodiscard]] bool CanStillBeAdjusted(entt::entity s);
/// CanStillBeAdjusted's rule with the turns since placing (0 when never placed), the scaffold's limit and the info's
/// gameTurnsAfterPlacingCanStillPickUp
[[nodiscard]] bool CanStillBeAdjustedAfter(uint32_t turnsSincePlaced, int32_t limit, uint32_t afterPlacingTurns);
/// Whether the current plan (or the candidate) is set, not deleted and offers a building
[[nodiscard]] bool HasPlan(entt::entity s, bool candidate);
/// The scaffold's town
[[nodiscard]] entt::entity GetTown(entt::entity s);
/// The owner workshop, or null. The workshops read and write it through these (the snap to a slot,
/// DeleteDependants)
[[nodiscard]] entt::entity GetOwner(entt::entity s);
void SetOwner(entt::entity s, entt::entity owner);
/// The workshop slot (flags bits 6-7); 0 for anything that is not a scaffold. The one slot helper of both sides
/// (ecs::workshops and ecs::scaffolds)
[[nodiscard]] int32_t GetSlot(entt::entity s);
/// Sets the workshop slot bits (slot & 3) in the flags
void SetSlot(entt::entity s, int32_t slot);

// ---- deletion -------------------------------------------------------------------------------------------------------

/// Deletes a scaffold: DeleteDependants below, then ecs::ToBeDeleted
void ToBeDeleted(entt::entity s);
/// The scaffold's own clean-up before the generic deletion; idempotent: leaves its site; RemoveFromWorkshop; drops the
/// phantom; deletes both plans. The dead-list hook of ecs::ToBeDeleted (abodes::OnToBeDeleted) calls this one for an
/// entity with components::Scaffold, so every generic deletion (a storage pit taking it, DestroyThingsInWay, the
/// physics' BackInMap outside the map) runs it; never call ToBeDeleted above from inside ecs::ToBeDeleted (it would
/// recurse). The original's unreachable site deletion (the site still set) is not ported
void DeleteDependants(entt::entity s);
/// Leaves the owner workshop: removed from its list when listed there; the owner is cleared either way
void RemoveFromWorkshop(entt::entity s);
/// Leaves the site and deletes its building; a missing or deleted plan or candidate is replaced by an empty one at pos
void RemoveOldBuildingSite(entt::entity s);

// ---- the building it offers -----------------------------------------------------------------------------------------

/// The town of the nearest abode within 50 m (not cut at the radius), even null; else the nearest town or town plan
/// below 50 m of every player
[[nodiscard]] entt::entity GetTownForBuilding(entt::entity s);
/// Chooses the candidate plan for `town` (scaffold_plans::ChoosePlanForScaffold) or, town-less, a town centre that
/// founds a new town. True when a candidate was found
bool ChoosePlan(entt::entity s, entt::entity town);
/// ChoosePlan then UpdatePhantomBuildingPointers; returns ChoosePlan's result
bool ChoosePlanAndUpdate(entt::entity s, entt::entity town);
/// The cross-fade between the current plan (shown) and the candidate (the newest)
void UpdatePhantomBuildingPointers(entt::entity s);
/// The scaffold becomes a site (a new town first for a founding town centre). True when a site was made
bool BuildBuilding(entt::entity s, std::optional<PlayerNames> player, bool force);
/// Makes the plan a building site of the town (force skips the fit test)
bool TryToBuildPlannedBuilding(entt::entity s, entt::entity town, bool force);
/// BuildBuilding(player, true) -> SetPhantomMeshAndPosition and the placed turn (used when an abode is destroyed by an
/// effect and by the script's SetActive)
void ForceBuildBuilding(entt::entity s, std::optional<PlayerNames> player);
/// The town plans under the new site go
void DeletePlannedBuildingsUnderMe(entt::entity s);
/// In the towns within maxDistanceForImpressingTowns (300 m), the first k villagers of the abodes' lists are impressed
/// by the reaction (belief only, no state change). The villagers' own reaction to the scaffold comes from
/// CreateReaction's spread (BuildBuilding)
void ImpressTowns(entt::entity s, uint32_t reaction);
/// The phantom takes the site's building mesh. (approximate) only the mesh is kept
void SetPhantomMeshAndPosition(entt::entity s);
/// SET_SCAFFOLD_PROPERTIES' scaffold part: the building type limit (raw), the value size truncated toward zero, the destroy
/// flag
void SetScaffoldProperties(entt::entity s, int32_t type, float size, bool destroy);

// ---- the hand (the hand code calls these; agreed API) --------------------------------------------------------------

/// No site -> yes; a site, still adjustable and its building not started -> yes; else no
[[nodiscard]] bool ValidForPlaceInHand(entt::entity s);
/// The hand took it: the holder is set and the placed turn cleared; the old site cancelled without a rebuild plan
/// (RemoveOldBuildingSite between Set/GetShouldNotBeAddedToPlanned); the owner workshop's ScaffoldMoved (slot = 1)
void OnPickedUp(entt::entity s, PlayerNames holder);
/// What ProcessInHand reads of the hand holding it
struct HandStatus
{
	PlayerNames player;
	/// Tested as != 0. (pending) its meaning (inferred: the turns the hand has been still / over the land)
	uint32_t stillTurns;
	/// The hand's synced position (not read by ProcessInHand itself)
	glm::vec3 hand;
};
/// Every turn while held: the feedback counter, the phantom's rotation, the cross-fade and the plan choice. Returns
/// true (the scaffold stays in the hand), always
bool ProcessInHand(entt::entity s, const HandStatus& status);
/// The scaffold leaves the hand: with the dropped-gently flag and a plan that needs fewer than its value, a new
/// scaffold of the surplus is made: the hand must put it in the hand. Returns it, or entt::null. Call it after the
/// physics' InitialisePhysicsFromHand (which sets the flag)
[[nodiscard]] entt::entity OnLeftHand(entt::entity s, PlayerNames holder);
/// Another scaffold that may combine with it, or a storage pit that stores wood
[[nodiscard]] bool ValidToApplyThisToObject(entt::entity s, entt::entity target);
/// A scaffold (two object state bits taken as clear) -> Combine, the combine spot visual and G_ScaffoldCombine at
/// `handPos` (with a hand), return 1: the HAND then starts its immersion effect; a storage pit that takes it
/// (take_resource::StoragePit) -> 3; else 0
int ApplyThisToObject(entt::entity s, entt::entity target, const pot_resource::Dropper& hand, const glm::vec3& handPos);
/// The plan info's ToolTipsForBuild when it has a plan, else the default drop tool tip
[[nodiscard]] uint32_t GetOverwriteDropToolTip(entt::entity s);
/// The pick-up tool tip
inline constexpr uint32_t k_OverwritePickUpToolTip = 0xEE7;
/// This one (in the hand) goes into o (on the ground); o survives with both values and this one's plan
void Combine(entt::entity s, entt::entity target);

// ---- the tap (ecs::hand_tap) ----------------------------------------------------------------------------------------

/// Value > 1, not flying, no site or still adjustable, free to offer any building
[[nodiscard]] bool ValidToTap(entt::entity s);
/// A site -> RemoveOldBuildingSite; Split; G_ScaffoldTap 151 + a counter at the hand's point. Returns 1
uint32_t Tap(entt::entity s, glm::vec3 handPos);
/// One scaffold of value 1 splits off at a random quarter turn, both get a body
void Split(entt::entity s);
/// The class's hand_tap handler (valid to tap / tap), once (Create calls it)
void RegisterTapHandler();

// ---- the physics (registered with PhysicsObjects::SetClassHandlers(PhysicsClass::Scaffold, ..)) -------------------

/// Snapped in a workshop slot (owner and slot state == 2) -> no; a site -> no; else as any mobile object (yes)
[[nodiscard]] bool CanBecomePhysicsObject(entt::entity s);
/// Before the generic object's InitialisePhysicsFromHand: the destroy flag -> DestroyThingsInWay
void OnBeforeInitialisePhysicsFromHand(entt::entity s);
/// After the generic object's InitialisePhysicsFromHand returned a body: dropped gently =
/// (po.flags & PhysicsObject::k_Landed) && !dontReplant
void OnInitialisePhysicsFromHand(entt::entity s, const physics::PhysicsObject& po, bool dontReplant);
/// After the generic InitialisePhysics: a body and an owner -> owner's ScaffoldMoved. The hand's path enters the
/// physics directly, so not from the hand
void OnInitialisePhysics(entt::entity s, physics::PhysicsObject& po, bool fromHand);
/// EndPhysics (the class calls PhysicsObjects::BackInMap itself): the snap to a workshop slot, or BuildBuilding for a
/// gentle drop from the hand, or the "no building" puff. Returns the entity that stays, or entt::null when it was
/// deleted out of the map
entt::entity OnEndPhysics(entt::entity s, physics::PhysicsObject& po);
/// The objects inside the plan's radius are pushed away (bodies) or deleted
void DestroyThingsInWay(entt::entity s);
/// The class's physics handlers in PhysicsObjects::SetClassHandlers(PhysicsClass::Scaffold, ..), once (Create calls
/// it): CanBecomeAPhysicsObject, InitialisePhysicsFromHand (before / after the generic one), InitialisePhysics,
/// EndPhysics (which calls PhysicsObjects::BackInMap itself: callsBackInMap)
void RegisterPhysicsHandlers();

} // namespace openblack::ecs::scaffolds
