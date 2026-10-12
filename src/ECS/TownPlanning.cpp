/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownPlanning.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <limits>
#include <optional>
#include <tuple>
#include <vector>

#include <LNDFile.h>
#include <glm/common.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/PlannedBuildingRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "ECS/Systems/Implementations/VillagerFootpath.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace town_planning = openblack::ecs::town_planning;
namespace planned_building = openblack::ecs::planned_building;

namespace
{

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

const GAbodeInfo& InfoOf(AbodeInfo info)
{
	return Locator::infoConstants::value().abode.at(static_cast<size_t>(info));
}

/// What a building going up counts as among its town's sites: an abode its kind, a temple or worship site a citadel
AbodeType SiteTypeOf(entt::entity building)
{
	auto& registry = Entities();
	if (const auto* abode = registry.TryGet<const Abode>(building); abode != nullptr && abode->info != AbodeInfo::None)
	{
		return InfoOf(abode->info).abodeType;
	}
	if (registry.AnyOf<Temple, WorshipSite>(building))
	{
		return AbodeType::Citadel;
	}
	return AbodeType::General;
}

planned_building::TownForPlans TownForPlans(entt::entity town)
{
	auto& registry = Entities();
	const auto& data = registry.Get<const Town>(town);
	planned_building::TownForPlans plans {
	    .neutral = data.owner == PlayerNames::NEUTRAL,
	    .homeless = static_cast<uint32_t>(data.homelessVillagers.size()),
	};
	if (const auto* stats = registry.TryGet<const TownStats>(town))
	{
		plans.adultRoom = stats->adultRoom;
		plans.adults = stats->adults;
		plans.children = stats->children;
		plans.built = stats->abodesByNumber;
	}
	if (Locator::townDesireSystem::has_value())
	{
		plans.wonderDesire = Locator::townDesireSystem::value().GetDesire(town, TownDesireInfo::ToBuildWonder);
	}
	plans.wonderTrigger = Locator::infoConstants::value()
	                          .townDesire.at(static_cast<size_t>(TownDesireInfo::ToBuildWonder))
	                          .desireTriggersVillagerAction;
	return plans;
}

/// How the town's want reads one of its plans; none for what isn't a plan
std::optional<planned_building::PlannedKind> KindOf(entt::entity plan, const std::vector<AbodeType>& siteTypes)
{
	auto& registry = Entities();
	planned_building::PlannedKind kind;
	if (const auto* abode = registry.TryGet<const PlannedAbode>(plan))
	{
		const auto& info = InfoOf(abode->info);
		kind = {.type = info.abodeType,
		        .number = info.abodeNumber,
		        .desireToBeBuilt = info.desireToBeBuilt,
		        .maxAdults = info.maxVillagersInAbode};
	}
	else if (registry.AllOf<PlannedTemple>(plan))
	{
		kind = {.type = AbodeType::Citadel, .desireToBeBuilt = Locator::infoConstants::value().citadelHeart.desireToBeBuilt};
	}
	else
	{
		return std::nullopt;
	}
	kind.sitesOfSameType = static_cast<uint32_t>(std::ranges::count(siteTypes, kind.type));
	return kind;
}

/// The plan of a type the town wants built most, none if it wants none. Plans that have gone are dropped from its list.
std::optional<entt::entity> BestPlan(entt::entity town, AbodeType wanted)
{
	auto& registry = Entities();
	auto& planned = registry.Get<Town>(town).planned;
	std::erase_if(planned, [&registry](entt::entity plan) {
		return !registry.Valid(plan) || !registry.AnyOf<PlannedAbode, PlannedTemple>(plan);
	});
	std::vector<AbodeType> siteTypes;
	for (const auto building : construction::SitesOfTown(town))
	{
		siteTypes.push_back(SiteTypeOf(building));
	}
	const auto plans = TownForPlans(town);
	std::vector<planned_building::PlannedCandidate> candidates;
	candidates.reserve(planned.size());
	for (const auto plan : planned)
	{
		const auto kind = KindOf(plan, siteTypes);
		candidates.push_back({.type = kind->type, .desire = planned_building::DesireToBeBuilt(*kind, plans)});
	}
	const auto best = planned_building::BestPlanned(candidates, wanted);
	return best.has_value() ? std::optional(planned[*best]) : std::nullopt;
}

/// A map position's cell as the game keeps it: the whole map units, truncated, over the units in a cell
uint32_t CellOfMetres(float metres)
{
	const auto whole = static_cast<int32_t>(metres * 65536.0f / gutils::k_MetresPerCell);
	return static_cast<uint32_t>(whole) >> 16;
}

/// Every other town, the players' in the order each gained them and then the neutral ones, with the box its buildings
/// and fields cover
std::vector<planned_building::OtherTown> OtherTowns(entt::entity town)
{
	auto& registry = Entities();
	std::vector<std::tuple<std::tuple<bool, uint8_t, uint32_t, uint32_t>, entt::entity>> order;
	registry.Each<const Town>([&](entt::entity entity, const Town& data) {
		if (entity != town)
		{
			order.emplace_back(
			    std::tuple(data.owner == PlayerNames::NEUTRAL, static_cast<uint8_t>(data.owner), data.gained, data.id), entity);
		}
	});
	std::ranges::stable_sort(order, {}, [](const auto& each) { return std::get<0>(each); });
	std::vector<planned_building::OtherTown> others;
	for (const auto& [rank, entity] : order)
	{
		const auto& data = registry.Get<const Town>(entity);
		const auto* at = registry.TryGet<const Transform>(entity);
		if (at == nullptr)
		{
			continue;
		}
		planned_building::OtherTown other {.cell = glm::ivec2(CellOfMetres(at->position.x), CellOfMetres(at->position.z))};
		// The box about its buildings and fields, each widened by its radius
		auto min = glm::vec2(std::numeric_limits<float>::max());
		auto max = glm::vec2(0.0f);
		bool any = false;
		const auto widen = [&](entt::entity thing) {
			const auto* placed = registry.TryGet<const Transform>(thing);
			if (placed == nullptr)
			{
				return;
			}
			const float radius = world_objects::SizeOf(thing).radius;
			min = glm::min(min, glm::xz(placed->position) - radius);
			max = glm::max(max, glm::xz(placed->position) + radius);
			any = true;
		};
		for (const auto abode : data.abodes)
		{
			if (registry.Valid(abode))
			{
				widen(abode);
			}
		}
		registry.Each<const Field>([&](entt::entity field, const Field& each) {
			if (each.town == static_cast<int>(data.id))
			{
				widen(field);
			}
		});
		if (any)
		{
			other.areaMin = glm::ivec2(CellOfMetres(min.x), CellOfMetres(min.y));
			other.areaMax = glm::ivec2(CellOfMetres(max.x), CellOfMetres(max.y));
		}
		others.push_back(other);
	}
	return others;
}

/// Whether the land at a place is dry: a cell of the land without water
bool DryLandAt(glm::vec3 position)
{
	if (!Locator::terrainSystem::has_value())
	{
		return false;
	}
	const auto cell = glm::floor(glm::xz(position) / gutils::k_MetresPerCell);
	if (cell.x < 0.0f || cell.y < 0.0f || cell.x >= static_cast<float>(map_coords::k_MapCells) ||
	    cell.y >= static_cast<float>(map_coords::k_MapCells))
	{
		return false;
	}
	const auto* land = Locator::terrainSystem::value().FindCell(glm::u16vec2(cell));
	return land != nullptr && land->properties.hasWater == 0;
}

/// Whether a new house may go up where a plan has it: near enough another town it may at once; elsewhere it needs dry
/// land and no building standing in its way
bool TakesNewHouse(entt::entity town, entt::entity plan)
{
	auto& registry = Entities();
	const auto& transform = registry.Get<const Transform>(plan);
	const glm::ivec2 cell(CellOfMetres(transform.position.x), CellOfMetres(transform.position.z));
	if (planned_building::WithinNearestOtherTown(cell, OtherTowns(town)))
	{
		return true;
	}
	const auto meshId = resources::HashIdentifier(InfoOf(registry.Get<const PlannedAbode>(plan).info).meshId);
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(meshId) || !Locator::entitiesMap::has_value())
	{
		return true;
	}
	const auto box = meshes.Handle(meshId)->GetBoundingBox();
	const auto halfSize = box.Size() * 0.5f;
	const float scale = transform.scale.x;
	const float reach = std::max(halfSize.x, halfSize.z) * scale;
	const auto outline = map_cells::OutlineOfBox(transform.position, transform.rotation, scale, box.Center(), halfSize);
	const auto& map = Locator::entitiesMap::value();
	std::vector<std::vector<planned_building::BuildingNearby>> nearby;
	for (const auto covered : map_cells::CellsCovered(outline))
	{
		auto& buildings = nearby.emplace_back();
		for (const auto thing : map.GetFixedInGridCell(MapInterface::CellId(covered)))
		{
			const auto* resident = registry.TryGet<const MapCellResident>(thing);
			const auto* placed = registry.TryGet<const Transform>(thing);
			if (resident == nullptr || !resident->coversOutline || placed == nullptr)
			{
				continue;
			}
			buildings.push_back({.distance = gutils::GetDistanceInMetres(transform.position, placed->position),
			                     .radius = systems::object_measures::TwoDRadius(registry, thing)});
		}
	}
	std::vector<planned_building::FootprintCell> footprint;
	footprint.reserve(nearby.size());
	for (const auto& buildings : nearby)
	{
		footprint.push_back({.buildings = buildings});
	}
	return planned_building::IsClearToBuild(footprint, DryLandAt(transform.position), reach);
}

/// A planned building goes up where it was planned, with nothing of it built and a site for its builders. The town's
/// people gather elsewhere if it stands on their gathering place.
bool Start(entt::entity town, entt::entity plan)
{
	auto& registry = Entities();
	if (registry.AllOf<PlannedTemple>(plan))
	{
		construction::StartPlannedTemple(plan, 0.0f);
		return true;
	}
	const auto planned = registry.Get<const PlannedAbode>(plan);
	const auto transform = registry.Get<const Transform>(plan);
	std::erase(registry.Get<Town>(town).planned, plan);
	const auto building = archetypes::AbodeArchetype::Create(planned.townId, transform.position, planned.info, planned.yAngle,
	                                                         transform.scale.x, 0, 0, 0.0f);
	// The footpaths laid to the plan lead to the building that goes up in its place
	if (building != entt::null)
	{
		villager_footpath::HandOverFootpaths(plan, building);
	}
	registry.Destroy(plan);
	if (building == entt::null)
	{
		return false;
	}
	if (Locator::townSystem::has_value())
	{
		Locator::townSystem::value().BuildingCreated(town, glm::xz(transform.position),
		                                             systems::object_measures::TwoDRadius(registry, building));
	}
	construction::OpenSite(building, 0.0f);
	return true;
}

} // namespace

bool town_planning::AskThisTurn(entt::entity town)
{
	auto& data = Entities().Get<Town>(town);
	const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	if (data.askedToBuildTurn == turn)
	{
		return false;
	}
	data.askedToBuildTurn = turn;
	return true;
}

bool town_planning::StartBestHouse(entt::entity town)
{
	const auto plan = BestPlan(town, AbodeType::LivingQuarters);
	return plan.has_value() && Entities().AllOf<PlannedAbode>(*plan) && TakesNewHouse(town, *plan) && Start(town, *plan);
}

bool town_planning::StartBestCivicBuilding(entt::entity town)
{
	const auto plan = BestPlan(town, AbodeType::Civic);
	return plan.has_value() && Start(town, *plan);
}
