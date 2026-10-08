/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownStats.h"

#include <charconv>

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include <entt/core/hashed_string.hpp>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Villager.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Villager/VillagerCore.h"
#include "InfoConstants.h"
#include "Locator.h"

// The town's statistics (TownStats.h)

namespace openblack::ecs::town_stats
{
using namespace components;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// Counts one villager of the town
void AddVillager(TownStats& stats, entt::entity villager, const Villager& v)
{
	// A child counts as a child; else as an adult
	if (villager::IsChild(villager))
	{
		++stats.children;
	}
	else
	{
		++stats.adults;
	}
	// The men / the women by the info's sex, children too (an abode's desire for a man or a woman reads them)
	const auto& info = villager::InfoOf(villager);
	if (info.sex == SexType::Female)
	{
		++stats.females;
	}
	else if (info.sex == SexType::Male)
	{
		++stats.males;
	}
	// The food for dinner += the info's foodReqiredForDinner (unsigned)
	stats.foodForDinner =
	    static_cast<float>(static_cast<double>(stats.foodForDinner) + static_cast<double>(info.foodReqiredForDinner));
	// The food and wood it carries (signed 16 bits)
	stats.foodCarried = stats.foodCarried + static_cast<float>(static_cast<int32_t>(v.resourceHeld.at(0)));
	stats.woodCarried = stats.woodCarried + static_cast<float>(static_cast<int32_t>(v.resourceHeld.at(1)));
	// A disciple: the count of its disciple type (a byte)
	if ((v.flags & Villager::k_FlagDisciple) != 0 && v.discipleType < stats.disciples.size())
	{
		auto& count = stats.disciples.at(v.discipleType);
		count = static_cast<uint8_t>(count + 1);
	}
}

/// Counts one abode of the town
void AddAbode(TownStats& stats, const Abode& abode, const GAbodeInfo* info)
{
	// (openblack) an abode without an info record counts with no places and no type
	const uint32_t maxVillagers = info != nullptr ? info->maxVillagersInAbode : 0;
	const uint32_t maxChildren = info != nullptr ? info->maxChildrenInAbode : 0;
	// The total places += both; the free child places, the abode count and the wonder count are not kept (no reader)
	stats.totalPlaces += maxVillagers + maxChildren;
	// The free adult places GetDesireToBeBuilt reads: the original adds the max villagers here and moves it as
	// villagers move in, move out or grow up; recomputed as max villagers - the adults housed. (not verified) that it
	// equals that book-keeping (the AbodeVillagers owner to confirm)
	stats.freeAdultPlaces += static_cast<int32_t>(maxVillagers) - static_cast<int32_t>(abode.adultCount);
	// With places: one more abode with places, its adult and child places
	if (maxVillagers + maxChildren != 0)
	{
		++stats.abodesWithPlaces;
		stats.adultPlaces += maxVillagers;
		stats.childPlaces += maxChildren;
	}
	// A civic building
	if (info != nullptr && IsCivic(info->abodeType))
	{
		++stats.civicBuildings;
	}
	// The count of its abode number (a byte)
	const auto number = info != nullptr ? info->abodeNumber : abode.type;
	if (const auto n = static_cast<size_t>(static_cast<int32_t>(number)); n < stats.abodesByNumber.size())
	{
		auto& count = stats.abodesByNumber.at(n);
		count = static_cast<uint8_t>(count + 1);
	}
}
} // namespace

bool IsCivic(AbodeType type)
{
	switch (static_cast<uint32_t>(type))
	{
	case 0x14:   // Totem
	case 0x24:   // StoragePit
	case 0x44:   // Creche
	case 0x84:   // Workshop
	case 0x100:  // Wonder
	case 0x204:  // Graveyard
	case 0x404:  // TownCentre
	case 0x1004: // FootballPitch
	case 0x2004: // SpellDispenser
		return true;
	default:
		return false;
	}
}

const GAbodeInfo* FindAbodeInfo(Tribe tribe, AbodeNumber number)
{
	// The abode info records in order: the first match of tribe and number
	for (const auto& info : Locator::infoConstants::value().abode)
	{
		if ((info.tribeType == tribe || info.tribeType == Tribe::NONE) && info.abodeNumber == number)
		{
			return &info;
		}
	}
	return nullptr;
}

const GAbodeInfo* AbodeInfoOf(entt::entity abode, Tribe tribe)
{
	auto& registry = Entities();
	const auto* component = registry.TryGet<const Abode>(abode);
	if (component == nullptr)
	{
		return nullptr;
	}
	const auto* mesh = registry.TryGet<const Mesh>(abode);
	const auto meshId = mesh != nullptr ? mesh->id : 0;
	const GAbodeInfo* byTribe = nullptr;
	for (const auto& info : Locator::infoConstants::value().abode)
	{
		if (info.abodeNumber != component->type)
		{
			continue;
		}
		if (MeshIdHash(info.meshId) == meshId)
		{
			return &info;
		}
		if (byTribe == nullptr && info.tribeType == tribe)
		{
			byTribe = &info;
		}
	}
	return byTribe;
}

entt::id_type MeshIdHash(MeshId mesh)
{
	// the decimal text HashIdentifier formats, hashed over its length
	std::array<char, 16> text {};
	const auto written = std::to_chars(text.data(), text.data() + text.size(), static_cast<uint32_t>(mesh));
	return entt::hashed_string::value(text.data(), static_cast<size_t>(written.ptr - text.data()));
}

std::vector<entt::entity> AbodesOf(entt::entity town)
{
	auto& registry = Entities();
	std::vector<entt::entity> abodes;
	const auto* t = registry.TryGet<const Town>(town);
	if (t == nullptr)
	{
		return abodes;
	}
	// each abode's creation index read once, then the same comparisons on it (so the same order, ties included)
	std::vector<std::pair<int64_t, entt::entity>> found;
	registry.Each<const Abode>([&](entt::entity entity, const Abode& abode) {
		if (abode.townId == t->id)
		{
			found.emplace_back(object_index::Of(entity), entity);
		}
	});
	std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
	abodes.reserve(found.size());
	for (const auto& [index, entity] : found)
	{
		abodes.push_back(entity);
	}
	return abodes;
}

TownStats Compute(entt::entity town)
{
	TownStats stats {};
	auto& registry = Entities();
	if (!registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return stats;
	}
	const auto* tribe = registry.TryGet<const Tribe>(town);
	const auto townTribe = tribe != nullptr ? *tribe : Tribe::CELTIC; // (openblack, guard) as GatherInputs
	registry.Each<const Villager>([&](entt::entity entity, const Villager& v) {
		if (v.town == town)
		{
			AddVillager(stats, entity, v);
		}
	});
	for (const auto abode : AbodesOf(town))
	{
		// An abode is counted once it is made functional: an abode under construction is out
		const auto& component = registry.Get<Abode>(abode);
		if (!component.addedToTownStats)
		{
			continue;
		}
		AddAbode(stats, component, AbodeInfoOf(abode, townTribe));
	}
	// The civic plans: each plan that is civic
	for (plans::PlanIndex i = 0; i < plans::PlansOf(town); ++i)
	{
		if (plans::IsCivic(town, i))
		{
			++stats.civicPlans;
		}
	}
	// The wood at the sites (GetWoodForStats) whose town is this town; the original also follows the sites' resources
	// as they change (the same piles). (approximate) summed in list order, the original in the order of the changes
	for (const auto site : building_sites::SitesOf(town))
	{
		if (building_sites::GetTown(site) == town)
		{
			stats.woodAtSites = stats.woodAtSites + static_cast<float>(building_sites::GetWoodForStats(site));
		}
	}
	return stats;
}
} // namespace openblack::ecs::town_stats
