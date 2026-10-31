/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownRemoval.h"

#include <algorithm>

#include "ECS/Components/Animal.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"

namespace openblack::ecs
{

using components::Animal;
using components::Reward;
using components::Town;
using components::TownDesireFlag;
using components::Villager;

town_removal::Left town_removal::LeftIn(const Registry& registry, entt::entity town)
{
	Left left;
	const auto* data = registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
	if (data == nullptr)
	{
		return left;
	}
	const auto valid = [&registry](entt::entity thing) { return registry.Valid(thing); };
	left.buildings = static_cast<size_t>(std::ranges::count_if(data->abodes, valid));
	left.plans = static_cast<size_t>(std::ranges::count_if(data->planned, valid));
	registry.Each<const Villager>([&left, town](const Villager& villager) {
		if (villager.town == town)
		{
			++left.people;
		}
	});
	return left;
}

town_removal::Dependants town_removal::DependantsOf(const Registry& registry, entt::entity town)
{
	Dependants dependants;
	registry.Each<const Animal>([&dependants, town](entt::entity animal, const Animal& data) {
		if (data.town == town)
		{
			dependants.animals.push_back(animal);
		}
	});
	registry.Each<const Reward>([&dependants, town](entt::entity chest, const Reward& data) {
		if (data.town == town)
		{
			dependants.rewards.push_back(chest);
		}
	});
	registry.Each<const TownDesireFlag>([&dependants, town](entt::entity flag, const TownDesireFlag& data) {
		if (data.town == town)
		{
			dependants.desireFlags.push_back(flag);
		}
	});
	return dependants;
}

void town_removal::Forget(Registry& registry, entt::entity town)
{
	if (!registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return;
	}
	const auto dependants = DependantsOf(registry, town);
	for (const auto animal : dependants.animals)
	{
		registry.Get<Animal>(animal).town = entt::null;
	}
	for (const auto chest : dependants.rewards)
	{
		registry.Destroy(chest);
	}
	for (const auto flag : dependants.desireFlags)
	{
		registry.Destroy(flag);
	}
	// The number of the town names no town from now on
	auto& towns = registry.Context().towns;
	if (const auto found = towns.find(registry.Get<const Town>(town).id); found != towns.end() && found->second == town)
	{
		towns.erase(found);
	}
	registry.Destroy(town);
	registry.SetDirty();
}

} // namespace openblack::ecs
