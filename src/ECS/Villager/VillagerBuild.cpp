/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerBuild.h"

#include <string>
#include <utility>

#include <fmt/format.h>

#include "Common/GUtilsDistance.h"
#include "ECS/Abodes.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Life.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/Systems/VillagerBuildingSitesInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerForester.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerSatisfy.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerTrace.h"
#include "ECS/VillagerAnimations.h"
#include "InfoConstants.h"
#include "Locator.h"

// The builders' states and decisions (VillagerBuild.h)

namespace openblack::ecs::villager
{
using namespace components;
namespace tq = town_queries;

namespace
{
/// The BUILDER disciple type
constexpr uint8_t k_DiscipleBuilder = 4;
/// The alignment's weight in the build factor and the ring point's reach
constexpr float k_PointTwo = 0.2f;
/// The build factor's cap
constexpr float k_MaxBuildFactor = 1.2f;
/// Beyond this distance the walk to the ring point takes the footpath
constexpr float k_FootpathDistance = 40.0f;
/// ReenterBuildingState's IsTouching margin
constexpr float k_TouchingMargin = 0.001f;
/// The ring's entries
constexpr int32_t k_RingSize = 0x80;
/// The look mode used when turning to face the building
constexpr uint32_t k_LookMode = 1;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

LivingAction* ActionOf(entt::entity villager)
{
	return Entities().TryGet<LivingAction>(villager);
}

/// The villager's town: a valid town entity with its component, or null
entt::entity TownOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	// (guard) the original only reads the town link: this stands in for the missing town unlinking
	if (v == nullptr || v->town == entt::null || !ecs::IsAvailable(v->town) || !Entities().AllOf<Town>(v->town))
	{
		return entt::null;
	}
	return v->town;
}

bool IsBuilderDisciple(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return v != nullptr && v->discipleType == k_DiscipleBuilder;
}

/// The villager's position as a MapCoords (x / z, altitude 0)
map_coords::MapCoords MeCoords(entt::entity villager)
{
	const auto me = tq::PosOf(villager);
	return {me.x, me.y, 0.0f};
}

/// The villager's position as a world point (for the land alignment and the forest finders)
glm::vec3 PositionOf(entt::entity villager)
{
	const auto* transform = Entities().TryGet<const Transform>(villager);
	return transform != nullptr ? transform->position : glm::vec3(0.0f);
}

glm::ivec2 Xz(const map_coords::MapCoords& pos)
{
	return {pos.x, pos.z};
}

void TraceIf(entt::entity villager, const std::string& line)
{
	if (TraceOn(villager))
	{
		Trace(villager, line);
	}
}

/// The building side, through the Locator
systems::VillagerBuildingSitesInterface& SiteSide()
{
	return Locator::villagerBuildingSites::value();
}

/// The walk to a ring point (GotoBuildingSite, ReenterBuildingState): strictly beyond 40 m it takes the footpath to the
/// site's building, else a wall-hugging walk; both end in ArrivesAtBuildingSite
void WalkToRingPoint(entt::entity villager, entt::entity site, glm::ivec2 pos, const char* who)
{
	const float d = tq::GetDistanceInMetres(tq::PosOf(villager), pos);
	const bool footpath = d > k_FootpathDistance;
	if (TraceOn(villager))
	{
		const auto* v = VillagerOf(villager);
		Trace(villager,
		      fmt::format("build {}: site {} ring {} ({}, {}) dist {:.3f} footpath {}", who, static_cast<uint32_t>(site),
		                  v != nullptr ? v->buildPosIndex : -1, pos.x, pos.y, d, footpath ? 1 : 0));
	}
	if (footpath)
	{
		SetupMoveToOnFootpath(villager, SiteSide().GetBuilding(site), pos, VillagerStates::ArrivesAtBuildingSite);
	}
	else
	{
		SetupMoveToWithHug(villager, tq::ToMetres(pos), VillagerStates::ArrivesAtBuildingSite);
	}
}

/// A random ring point of the site: the index is written into buildPosIndex
glm::ivec2 RandomBuildPos(entt::entity villager, entt::entity site)
{
	auto* v = VillagerOf(villager);
	int32_t index = v != nullptr ? v->buildPosIndex : 0;
	const auto pos = SiteSide().GetRandomBuildPos(site, villager, index);
	v = VillagerOf(villager);
	if (v != nullptr)
	{
		v->buildPosIndex = index;
	}
	return Xz(pos);
}
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

float BuildFactor(float alignment)
{
	// f = 1 - a x 0.2, every step in float precision
	const float product = alignment * k_PointTwo;
	const float fe = 1.0f - product;
	float f = fe;
	// Below 1 -> 1
	if (fe < 1.0f)
	{
		f = 1.0f;
	}
	// Strictly above 1.2 -> 1.2
	else if (f > k_MaxBuildFactor)
	{
		f = k_MaxBuildFactor;
	}
	return f;
}

uint32_t BuildAmount(float woodPerCycle, float factor, uint32_t pile)
{
	// The wood used per build cycle times the factor, truncated
	const float product = woodPerCycle * factor;
	const auto u = static_cast<uint32_t>(static_cast<int64_t>(product));
	// No more than the site's wood pile (unsigned compare)
	return pile < u ? pile : u;
}

float BuildStep(uint32_t amount, float woodValue)
{
	// The amount as a float, divided by the wood value, in float precision
	const auto u = static_cast<float>(static_cast<uint64_t>(amount));
	return u / woodValue;
}

bool BuildPosOk(int32_t index)
{
	// Outside the ring's entries the caller falls back to DecideWhatToDo
	return index >= 0 && index < k_RingSize;
}

bool NearRingPoint(float distance)
{
	// The walk happens only when strictly farther than 0.2
	return !(distance > k_PointTwo);
}

WoodWeights WoodSourceWeights(uint32_t stock, int16_t capacity, uint32_t maxWoodCarried, bool forBuilding)
{
	if (forBuilding)
	{
		// Forest 0.5; store 1 when the stock is above the capacity (unsigned compare), else 0. A negative capacity (more
		// than MaxWoodCarried held) is huge unsigned: 0 (literal)
		const auto cap = static_cast<uint32_t>(static_cast<int32_t>(capacity));
		return {stock > cap ? 1.0f : 0.0f, 0.5f};
	}
	// frac = 1 - (capacity + 1e-5) / (MaxWoodCarried + 1e-5); store frac, forest 1 - frac (CheckSatisfyWoodDesire's
	// mode)
	const float frac = DropOffFraction(capacity, maxWoodCarried);
	return {frac, 1.0f - frac};
}

WoodChoice ChooseWoodSource(WoodWeights weights, float storeDistance, std::optional<float> forestDistance, bool isBigForest,
                            float maxDistance)
{
	WoodChoice choice;
	// The store's distance modifier times its weight, stored as a float
	choice.store = gutils::GetDistanceModifier(storeDistance, maxDistance) * weights.store;
	// The forest's distance modifier times its weight, or 0 without a forest
	choice.forest = forestDistance ? gutils::GetDistanceModifier(*forestDistance, maxDistance) * weights.forest : 0.0f;
	// Store strictly better -> 1
	if (choice.store > choice.forest)
	{
		choice.how = 1;
	}
	// A non-zero forest score: 2 when the forest has a BigForest, else 3
	else if (choice.forest != 0.0f && forestDistance)
	{
		choice.how = isBigForest ? 2 : 3;
	}
	return choice;
}

// ---- the decision path -------------------------------------------------------------------------------------------

uint32_t CheckNeededForBuilding(entt::entity villager)
{
	// (openblack, guard) the original has no null test for the town (its callers have one)
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	auto& siteSide = SiteSide();
	// No building happening in the town -> 0
	if (!siteSide.IsBuildingHappening(town))
	{
		return 0;
	}
	// The best site near the villager; a BUILDER disciple also considers full sites
	const bool includeFull = IsBuilderDisciple(villager);
	const auto site = siteSide.GetBestBuildingSite(town, MeCoords(villager), includeFull);
	// A site and SetupBuildingObject(site) == 1 -> 1
	const uint32_t result = site != entt::null && SetupBuildingObject(villager, site) == 1 ? 1 : 0;
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("build: need {} (best of {}, includeFull {}) -> {}", static_cast<uint32_t>(site),
		                            building_sites::SitesOf(town).size(), includeFull ? 1 : 0, result));
	}
	return result;
}

uint32_t SetupBuildingObject(entt::entity villager, entt::entity site)
{
	// No town -> 0
	if (TownOf(villager) == entt::null)
	{
		return 0;
	}
	auto& siteSide = SiteSide();
	// No building for the site -> 0
	const auto building = siteSide.GetBuilding(site);
	if (building == entt::null)
	{
		return 0;
	}
	// Built and repaired -> 0
	if (siteSide.IsBuilt(building) && siteSide.IsRepaired(building))
	{
		return 0;
	}
	// A built building here is a repair; the rest is the build path unchanged (the wood needed is (1 - life) x WoodValue -
	// pile, BuildBy raises the life, Repaired deletes the site)
	if (TraceOn(villager) && siteSide.IsBuilt(building))
	{
		const bool needs = siteSide.NeedsBuilders(site);
		Trace(villager, fmt::format("repair: site {} building {} needs builders {} builders {} life {:.4f}",
		                            static_cast<uint32_t>(site), static_cast<uint32_t>(building), needs ? 1 : 0,
		                            siteSide.GetBuilderCount(site), life::LifeOf(building)));
	}
	// Something to push out of the building's clear area -> 1
	const float radius = siteSide.GetClearAreaRadius(site);
	if (CheckForClearArea(villager, tq::PosOf(building), radius) == 1)
	{
		return 1;
	}
	// Otherwise go for the wood
	return SetupGetBuildingSupplies(villager, site) == 1 ? 1 : 0;
}

uint32_t SetupBuildingObjectForBuilding(entt::entity villager, entt::entity building)
{
	// No town -> 0
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	auto& siteSide = SiteSide();
	// The building's site in the town's list
	auto site = siteSide.GetBuildingSiteInList(town, building);
	// None: AddBuildingSite (failure -> 0) and look in the list again. (openblack, guard) one retry only: the original
	// loops for ever when the added site has no building
	if (site == entt::null)
	{
		if (siteSide.AddBuildingSite(town, building) == entt::null)
		{
			villager::TraceFormatted(villager, "build: building {} AddBuildingSite failed -> 0",
			                         static_cast<uint32_t>(building));
			return 0;
		}
		site = siteSide.GetBuildingSiteInList(town, building);
		if (site == entt::null)
		{
			return 0;
		}
	}
	// SetupBuildingObject(site) == 1 -> 1
	return SetupBuildingObject(villager, site) == 1 ? 1 : 0;
}

uint32_t CheckForClearArea([[maybe_unused]] entt::entity villager, [[maybe_unused]] glm::ivec2 pos,
                           [[maybe_unused]] float radius)
{
	// The original visits the clear area for the nearest pushable object with an applied force above 0.05. (inferred,
	// equivalent) no object type is pushable, so nothing is found and the visit has no other side effect (no random
	// draw): 0. State 185 ARRIVE_AT_PUSH_OBJECT is never entered (not ported)
	return 0;
}

// ---- the town desires (VillagerSatisfy.h) ------------------------------------------------------------------------

uint32_t CheckSatisfyAbodesDesire(entt::entity villager)
{
	// CheckNeededForBuilding == 1 -> 1 (any site, not only an abode's: literal)
	if (CheckNeededForBuilding(villager) == 1)
	{
		return 1;
	}
	// (openblack, guard) the original does not test the town for null here
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	// One plan request per town turn: the flag is set before the request (also when it fails)
	auto& t = Entities().Get<Town>(town);
	if (t.requestedPlanThisTurn)
	{
		return 0;
	}
	t.requestedPlanThisTurn = true;
	// RequestANewAbode succeeds and CheckNeededForBuilding == 1 -> 1
	const bool requested = SiteSide().RequestANewAbode(town);
	villager::TraceFormatted(villager, "build: abodes desire: RequestANewAbode -> {}", requested ? 1 : 0);
	return requested && CheckNeededForBuilding(villager) == 1 ? 1 : 0;
}

uint32_t CheckSatisfyCivicBuildings(entt::entity villager)
{
	// CheckNeededForBuilding == 1 -> 1
	if (CheckNeededForBuilding(villager) == 1)
	{
		return 1;
	}
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	// One plan request per town turn: the flag is set first
	auto& t = Entities().Get<Town>(town);
	if (t.requestedPlanThisTurn)
	{
		return 0;
	}
	t.requestedPlanThisTurn = true;
	// RequestBestPlanned succeeds and CheckNeededForBuilding == 1 -> 1
	const bool requested = SiteSide().RequestBestPlanned(town);
	villager::TraceFormatted(villager, "build: civic desire: RequestBestPlanned -> {}", requested ? 1 : 0);
	return requested && CheckNeededForBuilding(villager) == 1 ? 1 : 0;
}

uint32_t CheckSatisfyToBuild(entt::entity villager)
{
	// No town -> 0
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	// The best site (full ones too for a BUILDER disciple); a site and SetupBuildingObject == 1 -> 1
	const auto site = SiteSide().GetBestBuildingSite(town, MeCoords(villager), IsBuilderDisciple(villager));
	return site != entt::null && SetupBuildingObject(villager, site) == 1 ? 1 : 0;
}

uint32_t CheckSatisfyToRepair(entt::entity villager)
{
	// No town -> 0
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	// The best repair site; a site and SetupBuildingObject == 1 -> 1. Only the repair sites with a desire > 0
	// (ProcessTownRepairs' ones): a rock-damaged house's site is ordinary and is repaired through To_Build instead
	// (literal: Repair_Town rises for it and this returns 0)
	const auto site = SiteSide().GetBestRepairBuildingSite(town);
	const uint32_t result = site != entt::null && SetupBuildingObject(villager, site) == 1 ? 1 : 0;
	villager::TraceFormatted(villager, "repair desire: best repair site {} -> {}", static_cast<uint32_t>(site), result);
	return result;
}

// ---- the wood supply ---------------------------------------------------------------------------------------------

uint32_t SetupGetBuildingSupplies(entt::entity villager, entt::entity site)
{
	// No town -> 0
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	auto& siteSide = SiteSide();
	// Not a valid site -> 0
	if (!siteSide.IsBuildingSiteValid(town, site))
	{
		return 0;
	}
	// ShouldIGetWood (it asks for the wood drop-off position itself, only where it needs it) false -> GotoBuildingSite
	const auto dropoff = [villager]() {
		const auto p = GetResourceDropoffPos(villager, ResourceType::Wood);
		return map_coords::MapCoords {p.x, p.y, 0.0f};
	};
	if (!siteSide.ShouldIGetWood(site, villager, dropoff))
	{
		villager::TraceFormatted(villager, "build: supplies site {} shouldGet 0 -> goto", static_cast<uint32_t>(site));
		return GotoBuildingSite(villager, site);
	}
	// (inferred) a game flag would send the villager to wait for wood instead, but it is never set: SetupWaitForWood /
	// 232 WAIT_FOR_WOOD are not ported
	const auto source = DecideHowToGetWood(villager, true);
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("build: supplies site {} shouldGet 1 how {} (store {:.9f} forest {:.9f})",
		                            static_cast<uint32_t>(site), source.how, source.store, source.forestScore));
	}
	switch (source.how)
	{
	case 1:
		// The store
		return GotoStoragePitForBuildingMaterials(villager, site);
	case 2:
	{
		// Remember the site and take the footpath to the BigForest's arrive point (53 ARRIVES_AT_BIG_FOREST); 1
		if (auto* v = VillagerOf(villager); v != nullptr)
		{
			v->buildingSite = site;
		}
		const auto arrive = BigForestArrivePos(source.bigForest, villager);
		SetupMoveToOnFootpath(villager, source.bigForest, tq::ToMapCoords({arrive.x, arrive.z}),
		                      VillagerStates::ArrivesAtBigForest);
		return 1;
	}
	case 3:
		// Remember the site and go to the forest (49 FORESTER_ARRIVES_AT_FOREST)
		if (auto* v = VillagerOf(villager); v != nullptr)
		{
			v->buildingSite = site;
		}
		return source.forest.has_value() ? VillagerGotoForest(villager, *source.forest, VillagerStates::ForesterArrivesAtForest)
		                                 : 0;
	default:
		return 0;
	}
}

WoodSource DecideHowToGetWood(entt::entity villager, bool forBuilding)
{
	WoodSource result;
	if (VillagerOf(villager) == nullptr)
	{
		return result;
	}
	const auto town = TownOf(villager);
	// pos = (0, 0, 0); D = the town info's maxDistanceForTownForest
	glm::ivec2 pos {0, 0};
	const float maxDistance = Locator::infoConstants::value().town.maxDistanceForTownForest;
	uint32_t stock = 0;
	// A functional storage pit (the town's or the home) -> its arrive position and its wood
	if (const auto pit = GetStoragePit(villager); pit != entt::null && abode_queries::IsFunctional(pit))
	{
		pos = abode_queries::GetArrivePos(pit);
		stock = object_resources::GetResource(pit, ResourceType::Wood);
	}
	else if (town != entt::null)
	{
		// The town's temporary wood pot (made if the town has none: a side effect, literal) and its wood. (openblack,
		// guard) no pot -> 0 (the original has no null test)
		const auto store = GetTemporaryStore(town, MeCoords(villager), ResourceType::Wood);
		pos = {store.pos.x, store.pos.z};
		if (store.pot != entt::null && ecs::IsAvailable(store.pot))
		{
			stock = object_resources::GetResource(store.pot, ResourceType::Wood);
		}
	}
	// (openblack, guard) without a town the original dereferences null: pos (0, 0), stock 0
	const auto weights = WoodSourceWeights(stock, GetWoodCapacity(villager), InfoOf(villager).maxWoodCarried, forBuilding);
	const auto me = tq::PosOf(villager);
	const float storeDistance = tq::GetDistanceInMetres(pos, me);
	// The town's nearest forest, else the global FindForest within D. (approximate) the town's forest list (filled by
	// the town features on load) is not filled yet, so the global FindForest decides
	const auto at = PositionOf(villager);
	std::optional<uint32_t> forest;
	if (town != entt::null)
	{
		forest = FindNearestForestToPos(Entities().Get<const Town>(town).id, at);
	}
	if (!forest)
	{
		forest = FindForest(at, maxDistance, false);
	}
	// The forest's distance and its BigForest
	std::optional<float> forestDistance;
	entt::entity bigForest = entt::null;
	if (forest)
	{
		const auto centre = ForestCentre(*forest);
		// (approximate) the forest's MapCoords from its centre in metres (a round trip: may be one fixed unit off);
		// openblack's Trees keeps no MapCoords for a forest
		forestDistance = tq::GetDistanceInMetres(tq::ToMapCoords({centre.x, centre.z}), me);
		bigForest = ForestBigForest(*forest);
	}
	const auto choice = ChooseWoodSource(weights, storeDistance, forestDistance, bigForest != entt::null, maxDistance);
	result.how = choice.how;
	result.store = choice.store;
	result.forestScore = choice.forest;
	if (choice.how == 2)
	{
		result.bigForest = bigForest;
	}
	else if (choice.how == 3)
	{
		result.forest = forest;
	}
	return result;
}

uint32_t GotoStoragePitForBuildingMaterials(entt::entity villager, entt::entity site)
{
	// No town or not a valid site -> 0
	const auto town = TownOf(villager);
	auto& siteSide = SiteSide();
	if (town == entt::null || !siteSide.IsBuildingSiteValid(town, site))
	{
		return 0;
	}
	// No room for more wood -> GotoBuildingSite(site)
	if (GetWoodCapacity(villager) <= 0)
	{
		TraceIf(villager, "build: pit for materials: full -> goto");
		return GotoBuildingSite(villager, site);
	}
	// The storage pit (kept even when it is not functional)
	const auto pit = GetStoragePit(villager);
	glm::ivec2 pos {0, 0};
	if (pit != entt::null && abode_queries::IsFunctional(pit))
	{
		// The pit's wood edge nearest to the villager
		pos = GetResourceNearestEdge(pit, ResourceType::Wood, villager);
	}
	else
	{
		// The wood drop-off position (may make the town's temporary wood pot)
		pos = GetResourceDropoffPos(villager, ResourceType::Wood);
	}
	// Not yet in the site's builder list
	if (!siteSide.IsBuilder(site, villager))
	{
		// The site needs no more builders and this is not a BUILDER disciple -> 0
		if (!siteSide.NeedsBuilders(site) && !IsBuilderDisciple(villager))
		{
			villager::TraceFormatted(villager, "build: pit for materials: site {} full -> 0", static_cast<uint32_t>(site));
			return 0;
		}
		// Unless the TOP state already leaves through ExitBuilding, remember the site
		if (!IsBuildingExitState(GetState(villager, Index::Top)))
		{
			if (auto* v = VillagerOf(villager); v != nullptr)
			{
				v->buildingSite = site;
			}
		}
	}
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("build: pit for materials: {} ({}, {}) -> 39", static_cast<uint32_t>(pit), pos.x, pos.y));
	}
	// A pit (or home), functional or not -> the footpath to it (literal); else a wall-hugging walk; both end in 39
	if (pit != entt::null)
	{
		SetupMoveToOnFootpath(villager, pit, pos, VillagerStates::ArrivesAtStoragePitForBuildingMaterials);
	}
	else
	{
		SetupMoveToWithHug(villager, tq::ToMetres(pos), VillagerStates::ArrivesAtStoragePitForBuildingMaterials);
	}
	return 1;
}

// ---- the site ----------------------------------------------------------------------------------------------------

uint32_t GotoBuildingSite(entt::entity villager, entt::entity site)
{
	// No town or not a valid site -> 0
	const auto town = TownOf(villager);
	auto& siteSide = SiteSide();
	if (town == entt::null || !siteSide.IsBuildingSiteValid(town, site))
	{
		return 0;
	}
	// Not a builder yet, the site needs no more and this is not a BUILDER disciple -> 0
	if (!siteSide.IsBuilder(site, villager) && !siteSide.NeedsBuilders(site) && !IsBuilderDisciple(villager))
	{
		villager::TraceFormatted(villager, "build goto: site {} full -> 0", static_cast<uint32_t>(site));
		return 0;
	}
	// SetTopState(163): a building state leaves through ExitBuilding (RemoveBuilder, the site cleared)
	SetTopState(villager, VillagerStates::DecideWhatToDo);
	// Remember the site, after the SetTopState
	if (auto* v = VillagerOf(villager); v != nullptr)
	{
		v->buildingSite = site;
	}
	// A random ring point (one GameFloatRand)
	const auto pos = RandomBuildPos(villager, site);
	// The walk with FINAL 40
	WalkToRingPoint(villager, site, pos, "goto");
	return 1;
}

uint32_t EnterBuilding(LivingAction& action, VillagerStates final, VillagerStates next)
{
	const auto villager = Entities().ToEntity(action);
	const auto* v = VillagerOf(villager);
	const auto town = TownOf(villager);
	const auto site = v != nullptr ? v->buildingSite : entt::null;
	auto& siteSide = SiteSide();
	// Not a valid site -> 0 (refused: the villager then enters 163). (openblack, guard) no town -> refused: the
	// original checks the site on a null town
	if (town == entt::null || !siteSide.IsBuildingSiteValid(town, site))
	{
		villager::TraceFormatted(villager, "build enter {} site {} refused", static_cast<uint32_t>(next),
		                         static_cast<uint32_t>(site));
		return 0;
	}
	// Coming from another entry function -> AddBuilder
	if (!IsStateEntryFunctionSameAs(final, next))
	{
		siteSide.AddBuilder(site, villager);
		if (TraceOn(villager))
		{
			Trace(villager, fmt::format("build enter {} site {} builders {}", static_cast<uint32_t>(next),
			                            static_cast<uint32_t>(site), siteSide.GetBuilderCount(site)));
		}
	}
	return 1;
}

uint32_t ExitBuilding(LivingAction& action, VillagerStates next)
{
	const auto villager = Entities().ToEntity(action);
	// The next state has the same exit function -> 1: the builder stays one
	if (IsStateExitFunctionSameAs(villager, next))
	{
		return 1;
	}
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 1;
	}
	const auto site = v->buildingSite;
	const auto town = TownOf(villager);
	auto& siteSide = SiteSide();
	// A town and a valid site -> RemoveBuilder
	if (town != entt::null && siteSide.IsBuildingSiteValid(town, site))
	{
		siteSide.RemoveBuilder(site, villager);
		if (TraceOn(villager))
		{
			Trace(villager, fmt::format("build exit {} site {} builders {}", static_cast<uint32_t>(next),
			                            static_cast<uint32_t>(site), siteSide.GetBuilderCount(site)));
		}
	}
	// Clear the site; 1
	if (auto* still = VillagerOf(villager); still != nullptr)
	{
		still->buildingSite = entt::null;
	}
	return 1;
}

uint32_t ArrivesAtStoragePitForBuildingMaterials(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto* v = VillagerOf(villager);
	const auto site = v != nullptr ? v->buildingSite : entt::null;
	const auto town = TownOf(villager);
	// No site or not a valid one -> SetTopState(163); 1. (openblack, guard) no town -> the same: the original checks the
	// site on a null town
	if (site == entt::null || town == entt::null || !SiteSide().IsBuildingSiteValid(town, site))
	{
		TraceIf(villager, "build 39: no valid site -> 163");
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// The room left for wood (signed)
	const int32_t cap = GetWoodCapacity(villager);
	// No room -> GotoBuildingSite(site) == 1 ? 1 : SetTopState(163), 1
	if (cap == 0)
	{
		TraceIf(villager, "build 39: cap 0 -> goto");
		if (GotoBuildingSite(villager, site) == 1)
		{
			return 1;
		}
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// Take the wood: a negative cap is passed as a huge unsigned amount (literal); then 184, else 163
	const auto held = v->resourceHeld.at(1);
	const auto r = ArrivesAtStoragePitForResource(villager, ResourceType::Wood, static_cast<uint32_t>(cap),
	                                              VillagerStates::ReenterBuildingState, VillagerStates::DecideWhatToDo);
	if (TraceOn(villager))
	{
		const auto* after = VillagerOf(villager);
		Trace(villager, fmt::format("build 39: cap {} -> +{} wood (pit {}) = {:#x}", cap,
		                            after != nullptr ? after->resourceHeld.at(1) - held : 0,
		                            static_cast<uint32_t>(GetStoragePit(villager)), r));
	}
	return r;
}

uint32_t ArrivesAtBuildingSite(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto* v = VillagerOf(villager);
	const auto site = v != nullptr ? v->buildingSite : entt::null;
	const auto town = TownOf(villager);
	auto& siteSide = SiteSide();
	// Not a valid site -> SetTopState(163); 1 (the site is kept: the exit clears it)
	if (v == nullptr || town == entt::null || !siteSide.IsBuildingSiteValid(town, site))
	{
		TraceIf(villager, "build 40: no valid site -> 163");
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// A ring index outside the ring -> 163
	const int32_t index = v->buildPosIndex;
	const auto ring = BuildPosOk(index) ? siteSide.GetBuildPos(site, index) : std::nullopt;
	if (!ring)
	{
		villager::TraceFormatted(villager, "build 40: ring {} -> 163", index);
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// The ring point in map coordinates
	const auto pos = Xz(*ring);
	// Farther than 0.2 m -> walk to it (FINAL 40); 1
	const float d = tq::GetDistanceInMetres(tq::PosOf(villager), pos);
	if (!NearRingPoint(d))
	{
		villager::TraceFormatted(villager, "build 40: ring {} dist {:.3f} -> walk", index, d);
		SetupMoveToWithHug(villager, tq::ToMetres(pos), VillagerStates::ArrivesAtBuildingSite);
		return 1;
	}
	// Turn to the building (none -> done); still turning -> 1
	const auto building = siteSide.GetBuilding(site);
	const uint32_t look = building != entt::null ? LookAtPos(villager, tq::PosOf(building), k_LookMode) : 1;
	if (look != 1)
	{
		return 1;
	}
	// Wood carried -> add it to the site's pile; DropWood(0) drops all of it, whatever was added (literal); then the
	// carried object is updated
	const auto wood = v->resourceHeld.at(1);
	uint32_t added = 0;
	if (wood != 0)
	{
		const auto me = MeCoords(villager);
		added = siteSide.AddResource(site, ResourceType::Wood, static_cast<uint32_t>(static_cast<int32_t>(wood)), &me);
		DropWood(villager, 0);
		if (auto* animation = Entities().TryGet<SkeletalAnimation>(villager); animation != nullptr && !animation->carriedLocked)
		{
			animation->carriedObject = SetStateCarriedObject(villager, animation->carriedObject);
		}
	}
	villager::TraceFormatted(villager, "build 40: ring {} dist {:.3f} look {} drop {} (added {})", index, d, look, wood, added);
	// PlayAnimThenSetState(41) (TOP 23 WAIT_FOR_ANIMATION, FINAL 41), keeping the turn count (so 23 ends when the 348
	// clip begun on entering 40 has played once)
	const uint16_t turns = action.turnsSinceStateChange;
	PlayAnimThenSetState(villager, VillagerStates::Building);
	if (auto* still = ActionOf(villager); still != nullptr)
	{
		still->turnsSinceStateChange = turns;
	}
	return 1;
}

uint32_t BuildingState(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto* v = VillagerOf(villager);
	// No town -> 0
	const auto town = TownOf(villager);
	if (v == nullptr || town == entt::null)
	{
		return 0;
	}
	const auto site = v->buildingSite;
	// Wait until the building clip has played once
	if (!VillagerAnimationDone(villager, action.turnsSinceStateChange))
	{
		return 1;
	}
	// One build cycle per clip
	action.turnsSinceStateChange = 0;
	auto& siteSide = SiteSide();
	// Clear the site FIRST (so the exit skips RemoveBuilder), then SetTopState(163); 1
	const auto release = [villager](const char* why) {
		if (auto* still = VillagerOf(villager); still != nullptr)
		{
			still->buildingSite = entt::null;
		}
		villager::TraceFormatted(villager, "build 41: release ({})", why);
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1u;
	};
	// Not a valid site -> release
	if (!siteSide.IsBuildingSiteValid(town, site))
	{
		return release("invalid");
	}
	// The land's alignment at the villager
	const float a = siteSide.LandAlignmentAt(PositionOf(villager));
	const float f = BuildFactor(a);
	// u = min(trunc(WoodUsedPerBuildCycle x f), the pile)
	const float perCycle = InfoOf(villager).woodUsedPerBuildCycle;
	const uint32_t pile = siteSide.GetResource(site, ResourceType::Wood);
	const uint32_t u = BuildAmount(perCycle, f, pile);
	// The building, kept for the IsBuilt test below
	const auto building = siteSide.GetBuilding(site);
	float value = 0.0f;
	float x = 0.0f;
	if (u != 0)
	{
		// x = u / the wood value, read BEFORE the removal
		value = siteSide.GetWoodValue(site);
		x = BuildStep(u, value);
		// Take the wood from the pile, the result ignored
		static_cast<void>(siteSide.RemoveResource(site, ResourceType::Wood, u));
		// Build the building by x (may finish it and delete the site)
		siteSide.BuildBy(site, x);
		// The town's wood used for building. TODO(stats): the player's game stats are not ported
		siteSide.AddWoodUsedForBuilding(town, u);
	}
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("build 41: a {:.6f} f {:.9f} u {} pile {} value {:.1f} x {:.9f} built {}", a, f, u, pile,
		                            value, x, building != entt::null ? abodes::GetPercentBuilt(building) : 0.0f));
	}
	// The site is no longer available -> release (it was deleted when built: the walk out ToBeDeleted gave this
	// villager is replaced by the 163 below, literal)
	if (!siteSide.IsAvailable(site))
	{
		return release("not available");
	}
	// Built and repaired -> release (without RemoveBuilder: literal)
	if (building != entt::null && siteSide.IsBuilt(building) && siteSide.IsRepaired(building))
	{
		return release("built");
	}
	// Wood left in the pile -> the next ring point (GameFloatRand then GameRand(2)) and a walk to it (FINAL 40); 1
	if (siteSide.GetResource(site, ResourceType::Wood) != 0)
	{
		auto* still = VillagerOf(villager);
		int32_t index = still != nullptr ? still->buildPosIndex : 0;
		const auto pos = Xz(siteSide.GetNextPosFromIndex(site, index));
		still = VillagerOf(villager);
		if (still != nullptr)
		{
			still->buildPosIndex = index;
		}
		villager::TraceFormatted(villager, "build 41: -> next {}", index);
		SetupMoveToWithHug(villager, tq::ToMetres(pos), VillagerStates::ArrivesAtBuildingSite);
		return 1;
	}
	// SetupGetBuildingSupplies(site): 0 leaves the builder in 41, hammering an empty site (literal)
	const auto r = SetupGetBuildingSupplies(villager, site);
	villager::TraceFormatted(villager, "build 41: -> supplies {}", r);
	return r;
}

uint32_t ReenterBuildingState(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto* v = VillagerOf(villager);
	const auto site = v != nullptr ? v->buildingSite : entt::null;
	const auto town = TownOf(villager);
	auto& siteSide = SiteSide();
	// No town or not a valid site -> SetTopState(163); 1
	if (town == entt::null || !siteSide.IsBuildingSiteValid(town, site))
	{
		TraceIf(villager, "build 184: no valid site -> 163");
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// ShouldIGetWood -> SetupGetBuildingSupplies == 1 ? 1 : SetTopState(163), 1
	const auto dropoff = [villager]() {
		const auto p = GetResourceDropoffPos(villager, ResourceType::Wood);
		return map_coords::MapCoords {p.x, p.y, 0.0f};
	};
	if (siteSide.ShouldIGetWood(site, villager, dropoff))
	{
		TraceIf(villager, "build 184: shouldGet 1");
		if (SetupGetBuildingSupplies(villager, site) == 1)
		{
			return 1;
		}
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// A random ring point (one GameFloatRand, also when touching)
	const auto pos = RandomBuildPos(villager, site);
	// Touching the building -> SetTopState(40); 1
	const auto building = siteSide.GetBuilding(site);
	const bool touching = building != entt::null && siteSide.IsTouching(villager, building, k_TouchingMargin);
	villager::TraceFormatted(villager, "build 184: shouldGet 0 touching {}", touching ? 1 : 0);
	if (touching)
	{
		SetTopState(villager, VillagerStates::ArrivesAtBuildingSite);
		return 1;
	}
	// The walk with FINAL 40
	WalkToRingPoint(villager, site, pos, "184");
	return 1;
}
} // namespace openblack::ecs::villager
