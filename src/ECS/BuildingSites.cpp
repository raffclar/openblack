/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BuildingSites.h"

#include <algorithm>

#include <glm/gtx/vec_swizzle.hpp>

#include "Common/GUtilsAngle.h"
#include "ECS/BuildingSiteRules.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "ECS/TempleConstruction.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// The tribal power the game divides a building's worth by: the fifth tribe's of the site's player. Each turn every
/// player's tribal power is put back to the most it has had and at least 1, and nothing raises that most, so it is 1.
constexpr float k_SiteTribalPower = 1.0f;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

std::optional<uint32_t> TownIdOf(entt::entity building)
{
	const auto& registry = Entities();
	if (const auto* temple = registry.TryGet<const Temple>(building); temple != nullptr && temple->town.has_value())
	{
		return static_cast<uint32_t>(*temple->town);
	}
	if (const auto* abode = registry.TryGet<const Abode>(building))
	{
		return abode->townId;
	}
	return std::nullopt;
}

const GMultiMapFixedInfo* InfoOf(entt::entity building)
{
	const auto& registry = Entities();
	const auto& info = Locator::infoConstants::value();
	if (registry.AllOf<Temple>(building))
	{
		return &info.citadelHeart;
	}
	if (const auto* abode = registry.TryGet<const Abode>(building); abode != nullptr && abode->info != AbodeInfo::None)
	{
		return &info.abode.at(static_cast<size_t>(abode->info));
	}
	return nullptr;
}

/// Whether the building stands whole: built, and its life full
bool IsBuiltAndRepaired(entt::entity building)
{
	return construction::IsBuilt(building) && world_objects::LifeOf(building) >= 1.0f;
}
} // namespace

std::vector<entt::entity> construction::SitesOfTown(entt::entity town)
{
	std::vector<entt::entity> sites;
	const auto& registry = Entities();
	if (!registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return sites;
	}
	const auto id = registry.Get<const Town>(town).id;
	registry.Each<const BuildingSite>([&](entt::entity building, const BuildingSite&) {
		if (TownIdOf(building) == id)
		{
			sites.push_back(building);
		}
	});
	return sites;
}

entt::entity construction::TownOfSite(entt::entity building)
{
	const auto id = TownIdOf(building);
	if (!id.has_value())
	{
		return entt::null;
	}
	entt::entity town = entt::null;
	Entities().Each<const Town>([&](entt::entity entity, const Town& candidate) {
		if (candidate.id == *id)
		{
			town = entity;
		}
	});
	return town;
}

bool construction::IsSiteValid(entt::entity town, entt::entity building)
{
	const auto& registry = Entities();
	if (town == entt::null || !registry.Valid(building) || !registry.AllOf<BuildingSite>(building) ||
	    TownOfSite(building) != town)
	{
		return false;
	}
	return !IsBuiltAndRepaired(building);
}

int32_t construction::MaxBuilders(entt::entity building)
{
	const auto* info = InfoOf(building);
	return info != nullptr ? static_cast<int32_t>(info->maxVillagerNeededToBuild) : 0;
}

int32_t construction::BuildersNeeded(entt::entity building)
{
	const auto* site = Entities().TryGet<const BuildingSite>(building);
	const auto* info = InfoOf(building);
	if (site == nullptr || info == nullptr)
	{
		return 0;
	}
	return building_site::BuildersNeeded({
	    .maxBuilders = static_cast<int32_t>(info->maxVillagerNeededToBuild),
	    .workers = site->workerCount,
	    .built = IsBuilt(building),
	    .repaired = world_objects::LifeOf(building) >= 1.0f,
	    // TODO(temple-builders): a built building's wish to be mended comes with repairing
	    .repairDesire = 0.0f,
	});
}

float construction::DesireForVillagers(entt::entity building)
{
	const auto* site = Entities().TryGet<const BuildingSite>(building);
	if (site == nullptr)
	{
		return 0.0f;
	}
	return building_site::DesireForVillagers(BuildersNeeded(building), MaxBuilders(building), site->desire);
}

uint32_t construction::WoodAtSite(entt::entity building)
{
	auto& registry = Entities();
	const auto* site = registry.TryGet<const BuildingSite>(building);
	if (site == nullptr)
	{
		return 0;
	}
	uint32_t wood = 0;
	for (const auto pile : site->piles)
	{
		if (!registry.Valid(pile))
		{
			continue;
		}
		if (const auto* pot = registry.TryGet<const Pot>(pile);
		    pot != nullptr &&
		    Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type)).resourceType == ResourceType::Wood)
		{
			wood += pot->amount;
		}
	}
	// TODO(temple-builders): the site of an abode going up keeps its wood in one pile of its own; abodes have no sites yet
	return wood;
}

float construction::SiteWoodValue(entt::entity building)
{
	const auto* info = InfoOf(building);
	const auto* transform = Entities().TryGet<const Transform>(building);
	if (info == nullptr || transform == nullptr)
	{
		return 0.0f;
	}
	return building_site::WoodValue(transform->scale.x, info->woodValue, k_SiteTribalPower);
}

float construction::WoodNeededToBuild(entt::entity building)
{
	const float builtOrLife = IsBuilt(building) ? world_objects::LifeOf(building) : BuiltOf(building);
	return building_site::WoodNeededToBuild(builtOrLife, SiteWoodValue(building), WoodAtSite(building));
}

float construction::BuildingRadius(entt::entity building)
{
	return systems::object_measures::TwoDRadius(Entities(), building);
}

glm::vec2 construction::NearestEdgeToPos(entt::entity building, glm::vec2 place)
{
	const auto centre = glm::xz(Entities().Get<const Transform>(building).position);
	const float angle = gutils::Get3DAngleFromXZ(centre, place);
	const auto offset = gutils::GetPointFromAngle(angle, BuildingRadius(building));
	return centre + glm::vec2(offset.x, offset.z);
}

void construction::AddWorker(entt::entity building, entt::entity villager)
{
	auto* site = Entities().TryGet<BuildingSite>(building);
	if (site == nullptr)
	{
		return;
	}
	if (villager != entt::null)
	{
		site->workers.push_back(villager);
	}
	++site->workerCount;
}

void construction::RemoveWorker(entt::entity building, entt::entity villager)
{
	auto* site = Entities().TryGet<BuildingSite>(building);
	// With nobody working at it the count stays as it is
	if (site == nullptr || site->workers.empty())
	{
		return;
	}
	std::erase(site->workers, villager);
	--site->workerCount;
}

std::optional<entt::entity> construction::StoragePitOf(entt::entity town)
{
	const auto& registry = Entities();
	if (!registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return std::nullopt;
	}
	const auto id = registry.Get<const Town>(town).id;
	std::optional<entt::entity> pit;
	registry.Each<const Abode, const StoragePit>([&](entt::entity entity, const Abode& abode, const StoragePit&) {
		if (abode.townId == id && !pit.has_value())
		{
			pit = entity;
		}
	});
	return pit;
}
