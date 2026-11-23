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

#include "ECS/BuildingSiteList.h"
#include "ECS/BuildingSiteRules.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/FixedKinds.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/TownCensus.h"
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
	// An altar has the row of its worship site
	if (const auto* altar = registry.TryGet<const WorshipAltar>(building))
	{
		const auto* site = registry.Valid(altar->site) ? registry.TryGet<const WorshipSite>(altar->site) : nullptr;
		return site != nullptr ? &info.worshipSite.at(static_cast<size_t>(site->tribe)) : nullptr;
	}
	if (const auto* abode = registry.TryGet<const Abode>(building); abode != nullptr && abode->info != AbodeInfo::None)
	{
		return &info.abode.at(static_cast<size_t>(abode->info));
	}
	if (registry.AllOf<SpellDispenser>(building))
	{
		return world_objects::AbodeInfoOf(building);
	}
	// A feature, an animated thing of the land and flowers have rows that hold the building values of the fixed things
	if (registry.AnyOf<Feature, AnimatedStatic>(building))
	{
		return static_cast<const GMultiMapFixedInfo*>(world_objects::InfoOf(building));
	}
	if (const auto* flowers = registry.TryGet<const Flowers>(building))
	{
		return &info.flowers.at(static_cast<size_t>(flowers->type));
	}
	return nullptr;
}

} // namespace

bool construction::IsBuiltAndRepaired(entt::entity building)
{
	const auto& registry = Entities();
	if (fixed_kinds::IsAlwaysWhole(registry, building))
	{
		return true;
	}
	// The altar of a worship site keeps how much of it is built
	if (const auto* altar = registry.TryGet<const WorshipAltar>(building))
	{
		return altar->built >= 1.0f && world_objects::LifeOf(building) >= 1.0f;
	}
	// The icon of a worship site is built when its site is built
	const auto* icon = registry.TryGet<const WorshipIcon>(building);
	const auto builtPart = icon != nullptr && registry.Valid(icon->site) ? icon->site : building;
	return IsBuilt(builtPart) && world_objects::LifeOf(building) >= 1.0f;
}

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
	return town_census::TownByNumber(Entities(), *id);
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
	// A thing with the mark that is not a building of the abodes, such as a worship site, the temple, a feature or
	// flowers, wants to be mended by its lost life alone, with no town. A thing that is always whole wants nothing.
	if (fixed_kinds::WantsRepairByLifeAlone(registry, building))
	{
		if (info == nullptr || fixed_kinds::IsAlwaysWhole(registry, building))
		{
			return 0.0f;
		}
		return town_desire::FixedDesireToBeRepaired(world_objects::LifeOf(building), info->desireToBeRepaired);
	}
	// The town wants to mend a miracle dispenser as one of its buildings with nobody in it
	if ((abode == nullptr && !registry.AllOf<SpellDispenser>(building)) || info == nullptr)
	{
		return 0.0f;
	}
	// A building with more life than the town needs to start a repair wants nothing. Most buildings are like this, so
	// the test comes before the search for the town. A life that is not a number goes on to the full rule.
	const auto& townInfo = Locator::infoConstants::value().town;
	const float life = world_objects::LifeOf(building);
	if (life > townInfo.thresholdToStartRepairing || TownOfSite(building) == entt::null)
	{
		return 0.0f;
	}
	const auto& abodeInfo = static_cast<const GAbodeInfo&>(*info);
	return town_desire::AbodeDesireToBeRepaired(
	    {
	        .life = life,
	        .livingQuarters =
	            (static_cast<uint32_t>(abodeInfo.abodeType) & static_cast<uint32_t>(AbodeType::LivingQuarters)) != 0,
	        .inhabitants = abode != nullptr ? static_cast<uint32_t>(abode->inhabitants.size()) : 0u,
	        .desireToBeRepaired = abodeInfo.desireToBeRepaired,
	    },
	    townInfo);
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

map_coords::MapCoords construction::NearestEdgeToPos(entt::entity building, const map_coords::MapCoords& place)
{
	return building_site::OutFrom(thing_map_place::Of(Entities(), building), place,
	                              systems::object_measures::TwoDRadius(Entities(), building));
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
	// The main storage pit of the town, the pit that last started to work
	const auto pit = registry.Get<const Town>(town).storagePit;
	if (pit == entt::null || !registry.Valid(pit) || !registry.AllOf<StoragePit>(pit))
	{
		return std::nullopt;
	}
	return pit;
}

std::vector<std::pair<uint32_t, entt::entity>> construction::MainStoragePits()
{
	const auto& registry = Entities();
	std::vector<std::pair<uint32_t, entt::entity>> pits;
	for (const auto [entity, town] : registry.Underlying().view<const Town>().each())
	{
		if (const auto pit = StoragePitOf(entity))
		{
			pits.emplace_back(town.id, *pit);
		}
	}
	return pits;
}
