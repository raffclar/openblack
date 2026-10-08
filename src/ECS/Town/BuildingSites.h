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

#include <functional>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/Town.h"
#include "Enums.h"

// The building side of plans and building sites. A site is an entity with components::BuildingSite; a plan is an entry
// of Town::plannedAbodes (oldest first) named by its index, which is valid until the list changes. The villager side
// (builders, wood) calls the functions below and passes its own data (the ring index, the drop-off point) by argument.

namespace openblack
{
struct GAbodeInfo;
struct GMultiMapFixedInfo;
} // namespace openblack

namespace openblack::ecs::plans
{
/// An index into Town::plannedAbodes
using PlanIndex = size_t;

/// The number of plans in the town's list
[[nodiscard]] size_t PlansOf(entt::entity town);
/// Appends the plan at the tail (oldest first) and stamps it with the creation turn; town_stats::Compute counts the
/// list. Returns its index
PlanIndex AddPlanned(entt::entity town, components::PlannedAbode plan);
/// Takes the plan out of the list (the later indexes move down)
void RemovePlanned(entt::entity town, PlanIndex plan);
/// The plan's abode type from its info; a citadel heart plan is a Citadel
[[nodiscard]] AbodeType GetAbodeType(entt::entity town, PlanIndex plan);
/// Whether the plan is civic: a civic abode type (town_stats::IsCivic's set); a town centre plan always; a citadel
/// heart plan never (its type has the civic bit, but the town stats do not count it)
[[nodiscard]] bool IsCivic(entt::entity town, PlanIndex plan);
/// The info's desireToBeRepaired when the plan was built, else 0
[[nodiscard]] float GetDesireToBeRepaired(entt::entity town, PlanIndex plan);
/// The plan's GAbodeInfo; null when the index or the info is out of range
[[nodiscard]] const GAbodeInfo* InfoOf(entt::entity town, PlanIndex plan);
/// The plan's position as a MapCoords (x and z; the altitude is not read) and its scale; a zero MapCoords / 0 for an
/// index out of range
[[nodiscard]] map_coords::MapCoords CoordsOf(entt::entity town, PlanIndex plan);
[[nodiscard]] float ScaleOf(entt::entity town, PlanIndex plan);

/// How much the town wants a building of that info, with n scaffolds offered (0 from GetBestPlanned). The switch on the
/// abode type and its constants are in the .cpp
[[nodiscard]] float GetDesireToBeBuilt(entt::entity town, const GAbodeInfo& info, uint32_t scaffolds);
/// The same for any GMultiMapFixedInfo (the citadel heart's has the Citadel type); `abode` is the GAbodeInfo when it is
/// one (the living quarters case reads it)
[[nodiscard]] float GetDesireToBeBuilt(entt::entity town, const GMultiMapFixedInfo& info, AbodeType abodeType,
                                       const GAbodeInfo* abode, uint32_t scaffolds);
/// best = 0; among the plans, oldest first, whose abode type matches the mask, the strictly larger
/// GetDesireToBeBuilt(info, 0) wins, the first on ties. nullopt: none above 0
[[nodiscard]] std::optional<PlanIndex> GetBestPlanned(entt::entity town, float& best, uint32_t mask);
/// best = r; for each plan (skipped: onlyRebuild and not built) v = distance(pos, plan) - (the plan's mesh 2D radius x
/// scale + r); v <= best -> best = v, it (ties: the LAST one). With r = 1 a plan is found within R + 2 m
[[nodiscard]] std::optional<PlanIndex> GetPlannedAtPos(entt::entity town, const map_coords::MapCoords& pos, float r,
                                                       bool onlyRebuild);
/// The placement check (town_placement) first: null when it fails; else CreatePlannedNoFixedCheck
entt::entity CreatePlanned(entt::entity town, PlanIndex plan, float life);
/// Creates the building under construction from the plan (a rebuild plan marks it not repaired; not for a town centre
/// plan) and deletes the plan. The life argument is ignored (always under construction). Returns the building or null
/// (the plan survives). A citadel heart plan goes to CitadelArchetype::CreatePlannedNoFixedCheck, the life passed on
entt::entity CreatePlannedNoFixedCheck(entt::entity town, PlanIndex plan, float life);
/// A plan where the building stands: its position, Y angle, scale, info, the creation turn, and wasBuilt from the
/// building's built bit (a rebuild plan when it was built). Always a plain abode plan (a town centre's too). The
/// footpath link should move to it (TODO(footpaths)); then AddPlanned. nullopt only without the building's info or
/// Transform (the original fails only on allocation)
std::optional<PlanIndex> CreateFromBuilding(entt::entity town, entt::entity building);
} // namespace openblack::ecs::plans

namespace openblack::ecs::building_sites
{
// ---- lifetime ------------------------------------------------------------------------------------------------------

/// A site on the building: a repair site when the building is marked not repaired; the building links back to it; the
/// repair base is the life, or 1.1 x life - 0.1 when the building has no destruction mesh and is built; the builders'
/// ring is computed. Not put in a town: InsertBuildingSite does that
entt::entity Create(entt::entity building);
/// Deletes the site, in the original's order (the .cpp). The builders are sent to DECIDE_WHAT_TO_DO (walking out of the
/// building first when inside); the pile is released. Reached through ecs::ToBeDeleted(site, now)
/// (abodes::OnToBeDeleted), which then marks the entity or destroys it
void ToBeDeleted(entt::entity site, bool now = false);
/// A valid entity with the component that is not being deleted
[[nodiscard]] bool IsAvailable(entt::entity site);
/// Clears the wood pile when it is no longer available; a citadel site does the same for its six slots
void Process(entt::entity site);

// ---- the town list ------------------------------------------------------------------------------------------------

/// The town's sites, head first (the newest first)
[[nodiscard]] const std::vector<entt::entity>& SitesOf(entt::entity town);
/// Whether the town has any building site
[[nodiscard]] bool IsBuildingHappening(entt::entity town);
/// Already in the list -> nothing (no pulse); else (the town stats are recomputed by town_stats) the site goes at the
/// head and the town is pulsed
void InsertBuildingSite(entt::entity town, entt::entity site);
/// Every entry of the site out of the list (the town stats are recomputed)
void RemoveBuildingSiteFromList(entt::entity town, entt::entity site);
/// Create(building), InsertBuildingSite. A site on an existing building (also for damage, town repairs and the
/// villagers' building work); it is a repair site only when the building was marked not repaired, by ProcessTownRepairs
/// or a rebuild plan's conversion (a rock-damaged house's is an ordinary one)
entt::entity AddBuildingSite(entt::entity town, entt::entity building);
/// plans::CreatePlanned(0.0) (WITH the fixed check), Create, InsertBuildingSite; null when the plan could not be built
entt::entity AddBuildingSiteFromPlan(entt::entity town, plans::PlanIndex plan);
/// The same with plans::CreatePlannedNoFixedCheck(0.0)
entt::entity AddBuildingSiteNoFixedCheck(entt::entity town, plans::PlanIndex plan);
/// The same with a plan that is in no town list (a scaffold's): the building made from the plan, then the site;
/// deleting the plan itself has nothing to unlink and is the caller's
entt::entity AddBuildingSiteNoFixedCheck(entt::entity town, const components::PlannedAbode& plan);
/// The first site whose GetBuilding is the building -> ToBeDeleted, true; else false
bool RemoveBuildingSite(entt::entity town, entt::entity building);
/// The site whose GetBuilding is the building, or null
[[nodiscard]] entt::entity GetBuildingSiteInList(entt::entity town, entt::entity building);
/// In the list, GetBuilding set and not (built and repaired)
[[nodiscard]] bool IsBuildingSiteValid(entt::entity town, entt::entity site);
/// The head first; score = distance(pos, the building's nearest edge to pos) x (GetDesireForVillagers x 0.9 + 0.1)
/// below 99999 (strict); the minimum wins (literal: it favours the sites that need FEWER builders). includeFull is set
/// for a villager that is a builder
[[nodiscard]] entt::entity GetBestBuildingSite(entt::entity town, const map_coords::MapCoords& pos, bool includeFull);
/// Among the repair sites, the strictly largest GetDesireToBeRepaired above 0 (first on ties)
[[nodiscard]] entt::entity GetBestRepairBuildingSite(entt::entity town);
/// What ProcessTownRepairs picks: the abode (it wins over a plan) or the plan; both empty: nothing
struct TownRepairChoice
{
	entt::entity abode {entt::null};
	std::optional<plans::PlanIndex> plan;
};
/// The two loops of ProcessTownRepairs sharing one best (0 to start, the strictly larger wins, the first on ties): the
/// built plans by GetDesireToBeRepaired, oldest first; then the town's abodes without a site and not marked not
/// repaired, by abodes::GetDesireToBeRepaired: an abode must beat the best plan
[[nodiscard]] TownRepairChoice ChooseTownRepair(entt::entity town);
/// Once a town turn: ChooseTownRepair; an abode -> marked not repaired, then AddBuildingSite (a repair site); else a
/// plan -> AddBuildingSiteFromPlan (with the fixed check). At most one site a town turn
void ProcessTownRepairs(entt::entity town);
/// GetBestPlanned(civic mask) -> AddBuildingSiteNoFixedCheck; true when a site was made. The once-a-turn flag is the
/// caller's (Town::requestedPlanThisTurn)
bool RequestBestPlanned(entt::entity town);
/// GetBestPlanned(living quarters mask) -> AddBuildingSiteFromPlan (with the fixed check); the type is not used
bool RequestANewAbode(entt::entity town, AbodeType unused);
/// Adds the wood to Town::woodUsedForBuilding (float; kept on the town, as the stats are recomputed). The player's game
/// stats are not ported
void AddWoodUsedForBuilding(entt::entity town, uint32_t wood);
/// Every town of every player, neutral included: GetPlannedAtPos(pos, 1.0, false) -> AddBuildingSiteNoFixedCheck -> the
/// site's desire boost = v (CHL BUILD_BUILDING)
void ForceBuildingOfPlannedAtPos(const map_coords::MapCoords& pos, float desire);
/// Each site of the town (the next read first): root null, not available, or built and repaired -> the site's
/// ToBeDeleted when it is available
void PruneSites(entt::entity town);

/// What TownDesire reads of the sites and plans, in the list orders (sites head first, plans oldest first):
/// GetDesireForVillagers (To_Build), the builder count and GetMaxBuilders (ModificationToBuild), the plans'
/// GetDesireToBeRepaired (Repair)
struct DesireInputs
{
	std::vector<float> siteDesires;
	std::vector<uint32_t> siteBuilders; ///< As TownDesire.h keeps it (uint32; the counter is signed here)
	std::vector<int32_t> sitePlaces;
	std::vector<float> planRepairDesires;
};
[[nodiscard]] DesireInputs DesireInputsOf(entt::entity town);

// ---- one site
// --------------------------------------------------------------------------------------------------------

[[nodiscard]] entt::entity GetRootBuilding(entt::entity site);
/// The root when it is available, else null
[[nodiscard]] entt::entity GetBuilding(entt::entity site);
[[nodiscard]] entt::entity GetTown(entt::entity site); ///< root ? root's town : null
/// How many more builders the site wants (signed, may be negative)
[[nodiscard]] int32_t GetBuildersNeeded(entt::entity site);
[[nodiscard]] bool NeedsBuilders(entt::entity site); ///< GetBuildersNeeded > 0
[[nodiscard]] int32_t GetBuilderCount(entt::entity site);
/// GetBuilding ? its info's MaxVillagerNeededToBuild : 0
[[nodiscard]] int32_t GetMaxBuilders(entt::entity site);
[[nodiscard]] bool IsBuilder(entt::entity site, entt::entity villager); ///< The villager is in the builder list
[[nodiscard]] float GetDesireForVillagers(entt::entity site);
[[nodiscard]] float GetDesireToBeRepaired(entt::entity site);
[[nodiscard]] float GetClearAreaRadius(entt::entity site);
[[nodiscard]] float GetPercentBuilt(entt::entity site);
[[nodiscard]] float GetRadius(entt::entity site);
[[nodiscard]] float GetWoodValue(entt::entity site);
[[nodiscard]] float GetWoodNeededToBuild(entt::entity site);
[[nodiscard]] bool IsRepairSite(entt::entity site);
[[nodiscard]] float GetRepairBase(entt::entity site);
void SetRepairBase(entt::entity site, float base);
/// Whether the villager should fetch wood for the site. `resourceDropoffPos` is the villager's wood drop-off point (the
/// villager code's), asked only where the original asks it
[[nodiscard]] bool ShouldIGetWood(entt::entity site, entt::entity villager,
                                  const std::function<map_coords::MapCoords()>& resourceDropoffPos);
/// The pile's amount of that resource, 0 without one
[[nodiscard]] uint32_t GetResource(entt::entity site, ResourceType type);
[[nodiscard]] uint32_t GetWoodForStats(entt::entity site); ///< GetResource(WOOD)
/// WOOD only: the pile (made when missing, CreatePileWood) takes the amount. `pos` is the villager's position; a
/// standard site ignores it. Returns what was added. A citadel site: pos null adds nothing; else the nearest pile
uint32_t AddResource(entt::entity site, ResourceType type, uint32_t amount, const map_coords::MapCoords* pos,
                     bool poisoned = false);
/// WOOD only: taken from the pile. Returns what was removed. `interfacePos`: the player interface's position when it
/// removes it (the citadel's site takes its nearest pile then; else its slots in order)
uint32_t RemoveResource(entt::entity site, ResourceType type, uint32_t amount,
                        const map_coords::MapCoords* interfacePos = nullptr);
/// Builds the building by `amount` (abodes::BuildBy). The builder passes the wood removed / GetWoodValue, the wood
/// value read BEFORE RemoveResource
void BuildBy(entt::entity site, float amount);
/// The ring entry for the angle, its index written
[[nodiscard]] map_coords::MapCoords GetNearestEdge(entt::entity site, float angle, int32_t& index);
/// A ring entry on the villager's side of the building, within +-45 deg (one GameFloatRand)
[[nodiscard]] map_coords::MapCoords GetRandomBuildPos(entt::entity site, entt::entity villager, int32_t& index);
/// A ring entry a few metres on from `index` (GameFloatRand then GameRand(2))
[[nodiscard]] map_coords::MapCoords GetNextPosFromIndex(entt::entity site, int32_t& index);
/// The ring entry as the villager reads it on arriving: (x x 6553.6, z x 6553.6, 0), truncated toward zero; only for 0 <= index
/// < 128, else nullopt
[[nodiscard]] std::optional<map_coords::MapCoords> GetBuildPos(entt::entity site, int32_t index);
/// The duplicate search is dead (literal): the villager goes at the head, the counter ++
void AddBuilder(entt::entity site, entt::entity villager);
/// Every entry of the villager out, then the counter -- once (also when it was not there)
void RemoveBuilder(entt::entity site, entt::entity villager);
/// The scaffold goes at the head of the list. The scaffold's link back to the site is written by the caller
/// (ecs::scaffolds, components::Scaffold's writer)
void AddScaffold(entt::entity site, entt::entity scaffold);
/// Every entry of the scaffold out; the scaffold's link back is not cleared (the callers do)
void RemoveScaffold(entt::entity site, entt::entity scaffold);
/// The site's scaffolds, the head first; empty for anything that is not a site
[[nodiscard]] const std::vector<entt::entity>& ScaffoldsOf(entt::entity site);
/// The wood pile (a standard site's, the position ignored; a citadel site: pos null -> null, else the slot nearest to
/// pos)
[[nodiscard]] entt::entity GetPileWood(entt::entity site, const map_coords::MapCoords* pos);
/// The site whose pile this is (as the pile's structure asks), or null
[[nodiscard]] entt::entity SiteOfPile(entt::entity pile);
/// A "Magic Wood" pot at GetResourcePosAndYAngle(WOOD, -1) when the site is available and has none
void CreatePileWood(entt::entity site);
/// Where the resource goes (a standard site; a worship site's pile at its local (9, 0, -50)); a citadel site: slot
/// `index` 22 m out. `angle` may be null
[[nodiscard]] map_coords::MapCoords GetResourcePosAndYAngle(entt::entity site, ResourceType type, int32_t index, float* angle);
/// Whether the pot is the site's: a citadel site's six slots; a standard site's pile; else false
[[nodiscard]] bool IsLinkedToThisBuildingSite(entt::entity site, entt::entity pot);
/// For each citadel slot i, GetResourcePosAndYAngle(WOOD, i), and an empty "Magic Wood" pot there when the slot is
/// empty or not available
void CreatePilesOfWood(entt::entity site);
/// Sets the site's desire boost (ForceBuildingOfPlannedAtPos)
void SetDesireBoost(entt::entity site, float boost);
} // namespace openblack::ecs::building_sites
