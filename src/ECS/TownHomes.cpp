/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownHomes.h"

#include <algorithm>
#include <vector>

#include "Common/GUtilsDistance.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// Past this distance a building is liked no less for being further off
constexpr float k_FarthestLiked = 500.0f;

void AddFirst(std::vector<entt::entity>& list, entt::entity entity)
{
	if (std::ranges::find(list, entity) == list.end())
	{
		list.insert(list.begin(), entity);
	}
}

void LeaveAbode(Registry& registry, const town_homes::Homes& homes, entt::entity abode, entt::entity villager)
{
	if (homes.leaving)
	{
		homes.leaving(villager);
	}
	if (auto* building = registry.Valid(abode) ? registry.TryGet<Abode>(abode) : nullptr; building != nullptr)
	{
		std::erase(building->inhabitants, villager);
	}
}

town_homes::Occupancy OccupancyOf(const Registry& registry, entt::entity abode, const town_homes::AbodeRoom& room,
                                  const Villager& newcomer, entt::entity villager)
{
	town_homes::Occupancy occupancy {.child = newcomer.lifeStage == Villager::LifeStage::Child,
	                                 .maxAdults = room.maxAdults,
	                                 .maxChildren = room.maxChildren};
	for (const auto resident : registry.Get<const Abode>(abode).inhabitants)
	{
		const auto* person = registry.Valid(resident) ? registry.TryGet<const Villager>(resident) : nullptr;
		if (person == nullptr)
		{
			continue;
		}
		++occupancy.people;
		if (person->lifeStage == Villager::LifeStage::Child)
		{
			++occupancy.children;
		}
		else
		{
			++occupancy.adults;
		}
		if (person->sex == newcomer.sex)
		{
			++occupancy.sameSex;
		}
	}
	const auto* from = registry.TryGet<const Transform>(abode);
	const auto* to = registry.TryGet<const Transform>(villager);
	if (from != nullptr && to != nullptr)
	{
		occupancy.distance = gutils::GetDistanceInMetres(from->position, to->position);
	}
	return occupancy;
}
} // namespace

using town_homes::Homes;
using town_homes::Occupancy;

float town_homes::ScoreForAddingVillager(const Occupancy& occupancy)
{
	const uint32_t most = occupancy.child ? occupancy.maxChildren : occupancy.maxAdults;
	if (most == 0)
	{
		return 0.0f;
	}
	const uint32_t living = occupancy.child ? occupancy.children : occupancy.adults;
	const float full = std::min(static_cast<float>(living) / static_cast<float>(most), 1.0f);
	float score = 1.0f - full;
	if (score > 0.0f)
	{
		// Liked better the fewer of its people share the newcomer's sex, and the nearer it is
		float sexes = 1.0f;
		if (occupancy.people != 0)
		{
			const float same = static_cast<float>(occupancy.sameSex) / static_cast<float>(occupancy.people);
			sexes = ((1.0f - same) + 1.0f) * 0.5f;
		}
		const float nearness = gutils::GetDistanceModifier(occupancy.distance, k_FarthestLiked);
		score = score * sexes * ((nearness + 1.0f) * 0.5f);
	}
	return score;
}

entt::entity town_homes::TownOfAbode(const Registry& registry, entt::entity abode)
{
	const auto* building = registry.Valid(abode) ? registry.TryGet<const Abode>(abode) : nullptr;
	if (building == nullptr)
	{
		return entt::null;
	}
	const auto& towns = registry.Context().towns;
	const auto found = towns.find(building->townId);
	if (found == towns.end() || !registry.Valid(found->second) || !registry.AllOf<Town>(found->second))
	{
		return entt::null;
	}
	return found->second;
}

entt::entity town_homes::FindAbodeWithSpace(const Registry& registry, const Homes& homes, entt::entity town,
                                            entt::entity villager, float leastScore)
{
	const auto* data = town != entt::null ? registry.TryGet<const Town>(town) : nullptr;
	const auto* newcomer = registry.TryGet<const Villager>(villager);
	if (data == nullptr || newcomer == nullptr)
	{
		return entt::null;
	}
	entt::entity best = entt::null;
	for (const auto abode : data->abodes)
	{
		if (!registry.Valid(abode) || !registry.AllOf<Abode>(abode))
		{
			continue;
		}
		const auto room = homes.room(abode);
		if (!room.has_value() || !room->functional)
		{
			continue;
		}
		const float score = ScoreForAddingVillager(OccupancyOf(registry, abode, *room, *newcomer, villager));
		if (score > leastScore)
		{
			leastScore = score;
			best = abode;
		}
	}
	return best;
}

bool town_homes::AddVillagerToTown(Registry& registry, const Homes& homes, entt::entity town, entt::entity villager)
{
	const auto* data = town != entt::null ? registry.TryGet<const Town>(town) : nullptr;
	if (data == nullptr || data->uninhabitable)
	{
		return false;
	}
	// TODO(villagers): the town's head counts take the villager in, and a town's first person sets up its worship site
	auto& person = registry.Get<Villager>(villager);
	person.town = town;
	const auto abode = person.abode;
	if (abode == entt::null || TownOfAbode(registry, abode) != town)
	{
		if (abode != entt::null)
		{
			LeaveAbode(registry, homes, abode, villager);
			person.abode = entt::null;
		}
		if (const auto home = FindAbodeWithSpace(registry, homes, town, villager, 0.0f); home != entt::null)
		{
			AddVillagerToAbode(registry, homes, home, villager);
			return true;
		}
		MakeHomeless(registry, homes, villager);
	}
	return true;
}

void town_homes::AddVillagerToAbode(Registry& registry, const Homes& homes, entt::entity abode, entt::entity villager)
{
	auto& person = registry.Get<Villager>(villager);
	const auto town = person.town;
	auto* homeless = town != entt::null ? registry.TryGet<Town>(town) : nullptr;
	if (homeless != nullptr && std::ranges::find(homeless->homelessVillagers, villager) != homeless->homelessVillagers.end())
	{
		std::erase(homeless->homelessVillagers, villager);
	}
	else if (person.abode != entt::null)
	{
		LeaveAbode(registry, homes, person.abode, villager);
	}
	AddFirst(registry.Get<Abode>(abode).inhabitants, villager);
	person.abode = abode;
	if (const auto abodeTown = TownOfAbode(registry, abode); abodeTown != entt::null && abodeTown != town)
	{
		AddVillagerToTown(registry, homes, abodeTown, villager);
	}
}

bool town_homes::MakeHomeless(Registry& registry, const Homes& homes, entt::entity villager)
{
	auto& person = registry.Get<Villager>(villager);
	if (person.abode != entt::null)
	{
		LeaveAbode(registry, homes, person.abode, villager);
		person.abode = entt::null;
	}
	auto* town = person.town != entt::null ? registry.TryGet<Town>(person.town) : nullptr;
	if (town == nullptr || std::ranges::find(town->homelessVillagers, villager) != town->homelessVillagers.end())
	{
		return false;
	}
	town->homelessVillagers.insert(town->homelessVillagers.begin(), villager);
	return true;
}
