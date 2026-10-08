/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>
#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack
{
struct GAbodeInfo;
// The info of a multi-cell static object; the struct keeps the name used by InfoConstants.h
struct GMultiMapFixedInfo;
} // namespace openblack

/// Every village building: the fields, the totem, the storage pit, the town centre and the spell dispenser included.
/// Covers the interface tap (knocking on a roof), life and damage, and construction.
namespace openblack::ecs::abodes
{

/// The ABODE_TYPE of the abode's info record. nullopt when no info record matches (openblack keeps the abode number and
/// the mesh, not the info pointer)
[[nodiscard]] std::optional<AbodeType> TypeOf(entt::entity abode);

/// Always true: every abode can be tapped
[[nodiscard]] bool InterfaceValidToTap(entt::entity abode);

/// A tap on the abode. `handPosition` is the hand's point. The original also remembers the abode's town and counts the
/// knock; it wakes the abode's villagers (all of them, any abode: VillagerEmergency.h) and, for a dwelling tapped by the
/// local player (`local`), plays the hand's knock at its own position (`handTransformPos`). The town and the count are
/// not ported.
void InterfaceTap(entt::entity abode, const glm::vec3& handPosition, bool local, glm::vec3 handTransformPos);
/// Registers InterfaceValidToTap / InterfaceTap in ecs::hand_tap (Register<Abode>). Once: later calls do nothing
/// (hand_tap::Register appends without checking). AbodeArchetype::Create calls it
void RegisterTapHandler();
/// The first entrance point of the given type in the abode's mesh (the NewEP block,
/// graphics::L3DMesh::GetNewEntrancePoints), through the abode's world matrix; none without a mesh, without the block
/// or without a point of the type
[[nodiscard]] std::optional<glm::vec3> GetEntrancePoint(entt::entity abode, int32_t type);
/// The last step of creating an abode: the did-you-know sign (ScriptHighlight row 1 at entrance point 1, with the
/// info's didYouKnow / dykCategory) and the street lantern at point 0 unless one is within 40 m (no shipped mesh has a
/// point 0)
void CreateAbodeSurroundingObjects(entt::entity abode);
/// Removes, when the abode is deleted, the script highlights in the cell of point 1 (when the info has a didYouKnow) and
/// the street lanterns in the cell of point 0, ToBeDeleted(0)
void DeleteAbodeSurroundingObjects(entt::entity abode);
/// (openblack) the generated DrawMesh models (construction and FragMesh) are erased when their DrawMesh goes, and the
/// FragMesh's model when its BuildingDamage goes (the real destruction of a deleted building): the listeners,
/// connected at every abode's creation (idempotent) so the physics can rely on them
void ConnectDrawMeshListener();

// ---- life and damage -----------------------------------------------------------------------------------------------

/// What the physics passes on an impact: the hitter and its player
struct PhysicalDamage
{
	/// The DestructionMesh's remaining part (FragMesh::GetRemaining) after the impact; nullopt without one (a building not
	/// built yet skips the FragMesh)
	std::optional<float> remaining;
	entt::entity hitter {entt::null};
	std::optional<PlayerNames> player;
	/// The thrower is a creature
	bool byCreature {false};
};
/// Damage from a physical impact: the crash sound, then effect 3 of info.dat (crush 1, alignment 1) applied by the
/// hitter. The player is the hitter's (none for a rock, the neutral player for another mobile static), else the hand's
/// (the town's aggressor). With a DestructionMesh it is scaled by max(life - remaining, 0) and divided by the defence
/// multiplier, then applied through effects::ApplyEffect (the damage is ReduceLife below, the alignment moves) and, at
/// life 0 from above, DestroyedByEffect. Without one (not built yet) the preset as it is: its crush times the crush
/// defence off the percent built. False when the building is gone
bool OnPhysicalDamage(entt::entity building, const PhysicalDamage& hit);
/// A building stops working: for an abode nothing besides the player's statistics (not ported); a storage pit also sets
/// up the pot reactions on its piles that hold something; a town centre sets the town's worship percentage to 0. The
/// villagers stay and no site is made here (ReduceLife makes it)
void StopBeingFunctional(entt::entity building, std::optional<PlayerNames> player);
/// A building destroyed by an effect: when its site has a scaffold it is started again from zero where the site's head
/// scaffold is (the old site removed, the building force-built); else ToBeDeleted. (not ported) the script abode's
/// keep path
void DestroyedByEffect(entt::entity building);
/// ToBeDeleted of a building: ecs::ToBeDeleted, whose building part is OnToBeDeleted below, then the map cells, the
/// physics and the registry (or the dead list)
void ToBeDeleted(entt::entity building);
/// The buildings' part of ToBeDeleted, the class's own unlinking. ecs::ToBeDeleted calls it for every entity at the
/// mark, before the map cells and before components::Unavailable (the flag is set last: the thing is still available
/// while it unlinks). It never calls ecs::ToBeDeleted on the entity itself (only on the piles, pots, totem and sites it
/// owns). Nothing for a planned abode (openblack's plans are not entities). Nothing for anything that is not one of
/// these:
/// - a scaffold: its own part before the mobile object's (scaffolds::DeleteDependants);
/// - a worship site: worship::site::ToBeDeleted, then the citadel part (the citadel's part list: none in openblack) and
///   the fixed object's part;
/// - an abode (the fields and the civic buildings too): the class's own part (storage pit: DeleteDependants and its six
///   piles; graveyard, creche, wonder: DeleteDependants; workshop: workshops::ToBeDeleted; field: nothing), then the
///   abode part: DeleteDependants once more, MoveAbodeToPlannedAbodes, out of the town (Abode::townId =
///   Abode::k_NoTown: out of every town list), and the fixed object's part (its reactions, its building site). Nothing
///   of the physics: neither the FragMesh nor the body is touched (the body leaves at the next turn; the FragMesh's
///   model at the real destruction, ConnectDrawMeshListener's listeners). `now` is ToBeDeleted's argument, handed on as
///   the original does to what goes with the thing: the pit's piles, the building site, the worship site's totem and
///   food pot. ecs::ToBeDeleted should pass its own (the engine's abode hook calls it with the default)
void OnToBeDeleted(entt::entity entity, bool now = false);
/// Whether the abode must not become a planned abode when deleted (set by the scaffolds); false for anything that is
/// not an abode
[[nodiscard]] bool GetShouldNotBeAddedToPlanned(entt::entity building);
/// Sets the flag read by GetShouldNotBeAddedToPlanned
void SetShouldNotBeAddedToPlanned(entt::entity building, bool value);
/// On deletion: no town -> false; unless GetShouldNotBeAddedToPlanned, a rebuild plan is made
/// (plans::CreateFromBuilding, marked as built when it was built) -> true; else the building site is removed from the
/// town and false
bool MoveAbodeToPlannedAbodes(entt::entity building);
/// The percent to draw: the smaller of GetPercentBuilt and GetPercentRepairedFromWhenDamaged
[[nodiscard]] float GetPercentForDrawBuilding(entt::entity building);
// ---- construction (docs/bw1-notes/buildings.md) ---------------------------------------------------------------------

/// An abode is built when it is not flagged as under construction and GetPercentBuilt >= 1; a Feature the same on its
/// percentBuilt (openblack's Feature keeps no flags); the citadel heart and a worship site the same as an abode on
/// components::CitadelPartBuild; any other fixed object is always built
[[nodiscard]] bool IsBuilt(entt::entity building);
/// An abode and a citadel part are repaired when GetPercentRepaired (the life) >= 1; anything else always is
[[nodiscard]] bool IsRepaired(entt::entity building);
/// The percent built of an abode or a Feature; 1 for anything else
[[nodiscard]] float GetPercentBuilt(entt::entity building);
/// The life (ecs::life)
[[nodiscard]] float GetPercentRepaired(entt::entity building);
/// The life below which an abode stops working: the info's thresholdForStopBeingFunctional; 0.75 for anything else
/// (also an abode without an info record)
[[nodiscard]] float GetPercentRepairedForNonFunctional(entt::entity building);
/// Whether the building has a DestructionMesh: the physics' BuildingDamage with its FragMesh
[[nodiscard]] bool HasDestructionMesh(entt::entity building);
/// The building site (components::BuildingSite's entity) or null
[[nodiscard]] entt::entity GetBuildingSite(entt::entity building);
/// Whether the building has a building site, so it draws as a partial build
[[nodiscard]] bool IsDrawBuilding(entt::entity building);
/// Not built -> 1; with a DestructionMesh (the physics' BuildingDamage) and a site: a = 1 - site start,
/// b = GetPercentRepaired - site start, (a == 0 || b == 0) ? 0 : b / a; else GetPercentRepaired x 0.98
[[nodiscard]] float GetPercentRepairedFromWhenDamaged(entt::entity building);
/// The abode's GAbodeInfo: the record AbodeArchetype made it with, else its number and mesh's
/// (town_stats::AbodeInfoOf with its town's tribe); null when none
[[nodiscard]] const GAbodeInfo* InfoOf(entt::entity building);
/// The building's info as a GMultiMapFixedInfo: an abode's GAbodeInfo (InfoOf), the citadel heart's
/// GCitadelHeartInfo, a worship site's GWorshipSiteInfo; null otherwise (read by building_sites)
[[nodiscard]] const GMultiMapFixedInfo* MultiCellStaticInfoOf(entt::entity building);
/// Whether the building casts its shadow on the land texture: false for an abode not built yet (turned off at creation
/// for an unbuilt one, on again when built); true otherwise, drawn or not (RenderingSystem's CastsStaticShadow calls
/// it; the physics' CastsPhysicsShadow may)
[[nodiscard]] bool CastsShadowOnTexture(entt::entity building);

/// Building work of `amount`: built and not repaired -> IncreaseLife and, at life >= 1, Repaired; not built -> the
/// percent built grows by the amount (0 when negative), >= 1 -> Built. Then RedrawConstruction
void BuildBy(entt::entity building, float amount);
/// Sets the percent built (0 when negative); >= 1 -> Built. Then RedrawConstruction
void SetPercentBuilt(entt::entity building, float percent);
/// The building is finished, in order: the site's ToBeDeleted; the "new building" reaction of a civic one (not ported);
/// the shadow-on-texture bake (not ported); the flags set to built, the percent to 1; the player's statistics (not
/// ported; multiplayer part skipped); MakeFunctional with a town. True
bool Built(entt::entity building);
/// The building starts working, with the class parts (storage pit: the town's storage pit; creche: the town's creche;
/// town centre: the totem and the spell icons; the graveyard's town link (inferred)). The order in the .cpp
void MakeFunctional(entt::entity building);
/// Repair finished: the site's ToBeDeleted, the damage removed (physics::Buildings::RemoveDamage), the damaged flag
/// cleared, MakeFunctional with a town. True
bool Repaired(entt::entity building);
/// Raises the life by `amount` (capped at 1); crossing the stop-functional threshold upwards calls
/// RestartBeingFunctional. Returns the new life
float IncreaseLife(entt::entity building, float amount);
/// The building works again: nothing for an abode; a storage pit removes the pot reactions on its available piles (the
/// food pile, the five wood piles)
void RestartBeingFunctional(entt::entity building);
/// Whether damage to the building puts its town in an emergency: a storage pit and a town centre do, other abodes not
[[nodiscard]] bool CausesTownEmergencyIfDamaged(entt::entity building);
/// Lowers the building's life (docs/bw1-notes/buildings.md): built -> the life; not built -> the percent built minus the
/// amount (>= 0) through SetPercentBuilt, at 0 the life too; then the stop-being-functional part with the town's
/// emergency (town_emergency::SetInStateOfEmergency for a storage pit or a town centre, also an unbuilt one at 0 %) and
/// the building site (an ordinary site, repaired by the builders of GetBestBuildingSite). A field changes nothing.
/// Below 1, each inhabitant reacts as to a tap on the abode. Returns the new life. The physics', the effects', the
/// fire's and the beam's damage comes here
float ReduceLife(entt::entity building, float amount, std::optional<PlayerNames> player);
/// town_desire::AbodeDesireToBeRepaired on this abode; 0 for anything else
[[nodiscard]] float GetDesireToBeRepaired(entt::entity building);

/// The partly built model of an abode with a building site and no DestructionMesh: drawn at GetPercentForDrawBuilding
/// (physics::PartialBuild::BuildMesh) into components::DrawMesh, and nothing at 0 (components::NotDrawn; the footprint
/// stays). The Mesh component stays the whole model (sizes, map cells, type). Rebuilt only when the percent changed;
/// both go when IsDrawBuilding no longer holds. With a FragMesh the physics' RedrawBuilding draws it
void RedrawConstruction(entt::entity building);

/// The script property BUILT_PERCENTAGE of an abode: GetPercentBuilt; nullopt for anything else (feature_build answers
/// for the Features)
[[nodiscard]] std::optional<float> GetBuiltPercentage(entt::entity entity);
/// The script setter of BUILT_PERCENTAGE on an abode: SetPercentBuilt. (pending) the town's building list part is not
/// read. False when it is not an abode
bool SetBuiltPercentage(entt::entity entity, float value);

} // namespace openblack::ecs::abodes
