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
#include "ECS/BuildingSiteList.h"
#include "ECS/BuildingSiteRules.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/TownDesire.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{

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
	if (const auto* dispenser = registry.TryGet<const SpellDispenser>(building))
	{
		const auto* town = registry.Valid(dispenser->town) ? registry.TryGet<const Town>(dispenser->town) : nullptr;
		return town != nullptr ? std::optional(town->id) : std::nullopt;
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
	if (const auto* site = registry.TryGet<const WorshipSite>(building))
	{
		return &info.worshipSite.at(static_cast<size_t>(site->tribe));
	}
	if (const auto* abode = registry.TryGet<const Abode>(building); abode != nullptr && abode->info != AbodeInfo::None)
	{
		return &info.abode.at(static_cast<size_t>(abode->info));
	}
	if (registry.AllOf<SpellDispenser>(building))
	{
		return world_objects::AbodeInfoOf(building);
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
	registry.Each<const BuildingSites>([&](entt::entity building, const BuildingSites& atBuilding) {
		if (building_site_list::Find(atBuilding, town) != nullptr)
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

BuildingSite* construction::SiteOf(entt::entity town, entt::entity building)
{
	auto& registry = Entities();
	auto* sites = town != entt::null && registry.Valid(building) ? registry.TryGet<BuildingSites>(building) : nullptr;
	return sites != nullptr ? building_site_list::Find(*sites, town) : nullptr;
}

BuildingSite* construction::BroughtWoodSite(entt::entity building)
{
	auto& registry = Entities();
	auto* sites = registry.Valid(building) ? registry.TryGet<BuildingSites>(building) : nullptr;
	return sites != nullptr ? building_site_list::TakingBroughtWood(*sites) : nullptr;
}

bool construction::IsBeingBuilt(entt::entity thing)
{
	const auto& registry = Entities();
	if (!registry.Valid(thing))
	{
		return false;
	}
	const auto* progress = registry.TryGet<const BuildProgress>(thing);
	return (progress != nullptr && progress->built < 1.0f) ||
	       (registry.AllOf<RepairSite>(thing) && world_objects::LifeOf(thing) < 1.0f);
}

bool construction::IsSiteValid(entt::entity town, entt::entity building)
{
	return SiteOf(town, building) != nullptr && !IsBuiltAndRepaired(building);
}

int32_t construction::MaxBuilders(entt::entity building)
{
	const auto* info = InfoOf(building);
	return info != nullptr ? static_cast<int32_t>(info->maxVillagerNeededToBuild) : 0;
}

int32_t construction::BuildersNeeded(entt::entity town, entt::entity building)
{
	const auto* site = SiteOf(town, building);
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
	    .repairDesire = DesireToBeRepaired(building),
	});
}

float construction::DesireForVillagers(entt::entity town, entt::entity building)
{
	const auto* site = SiteOf(town, building);
	if (site == nullptr)
	{
		return 0.0f;
	}
	return building_site::DesireForVillagers(BuildersNeeded(town, building), MaxBuilders(building), site->desire);
}

float construction::DesireToBeRepaired(entt::entity building)
{
	const auto& registry = Entities();
	const auto* abode = registry.TryGet<const Abode>(building);
	const auto* info = InfoOf(building);
	// The town wants to mend a miracle dispenser as one of its buildings with nobody in it
	if ((abode == nullptr && !registry.AllOf<SpellDispenser>(building)) || info == nullptr ||
	    TownOfSite(building) == entt::null)
	{
		return 0.0f;
	}
	const auto& abodeInfo = static_cast<const GAbodeInfo&>(*info);
	return town_desire::AbodeDesireToBeRepaired(
	    {
	        .life = world_objects::LifeOf(building),
	        .livingQuarters =
	            (static_cast<uint32_t>(abodeInfo.abodeType) & static_cast<uint32_t>(AbodeType::LivingQuarters)) != 0,
	        .inhabitants = abode != nullptr ? static_cast<uint32_t>(abode->inhabitants.size()) : 0u,
	        .desireToBeRepaired = abodeInfo.desireToBeRepaired,
	    },
	    Locator::infoConstants::value().town);
}

bool construction::IsRepairSite(entt::entity town, entt::entity building)
{
	const auto* site = SiteOf(town, building);
	return site != nullptr && site->repair;
}

float construction::SiteDesireToBeRepaired(entt::entity town, entt::entity building)
{
	return DesireForVillagers(town, building) * DesireToBeRepaired(building);
}

uint32_t construction::WoodAtSite(entt::entity town, entt::entity building)
{
	const auto* site = SiteOf(town, building);
	return site != nullptr ? WoodAt(*site) : 0u;
}

uint32_t construction::WoodAt(const BuildingSite& site)
{
	auto& registry = Entities();
	uint32_t wood = 0;
	for (const auto pile : site.piles)
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
	// A site belongs to the neutral player, so the game divides by the power that the Egyptian tribe gives the neutral
	// player (vanilla-bugs row 745)
	const float tribalPower = Locator::magicSystem::has_value()
	                              ? Locator::magicSystem::value().GetTribalPower(PlayerNames::NEUTRAL, Tribe::EGYPTIAN)
	                              : 1.0f;
	return building_site::WoodValue(transform->scale.x, info->woodValue, tribalPower);
}

float construction::WoodNeededToBuild(entt::entity town, entt::entity building)
{
	const float builtOrLife = IsBuilt(building) ? world_objects::LifeOf(building) : BuiltOf(building);
	return building_site::WoodNeededToBuild(builtOrLife, SiteWoodValue(building), WoodAtSite(town, building));
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

void construction::AddWorker(entt::entity town, entt::entity building, entt::entity villager)
{
	auto* site = SiteOf(town, building);
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

void construction::RemoveWorker(entt::entity town, entt::entity building, entt::entity villager)
{
	auto* site = SiteOf(town, building);
	if (site == nullptr)
	{
		return;
	}
	// Every time it was counted goes, but the count goes down by one, even with nobody on the list
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
	// The first storage pit of the town in the order of the walk of the pits
	return registry.FindFirst<const Abode, const StoragePit>(
	    [id](entt::entity, const Abode& abode, const StoragePit&) { return abode.townId == id; });
}

std::vector<std::pair<uint32_t, entt::entity>> construction::FirstStoragePits()
{
	const auto& registry = Entities();
	// The walk of the pits in the same order as StoragePitOf, so that the first pit of each town is the same
	std::vector<std::pair<uint32_t, entt::entity>> pits;
	for (const auto [entity, abode, pit] : registry.Underlying().view<const Abode, const StoragePit>().each())
	{
		if (std::ranges::none_of(pits, [id = abode.townId](const auto& kept) { return kept.first == id; }))
		{
			pits.emplace_back(abode.townId, entity);
		}
	}
	return pits;
}
