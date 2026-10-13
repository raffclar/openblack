/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleHeartTowns.h"

#include <algorithm>

#include "ECS/Components/Field.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Town.h"
#include "ECS/Registry.h"

namespace openblack::ecs::temple_heart_towns
{

using components::BuildProgress;
using components::Field;
using components::Town;

std::vector<physics::temple_heart::Town> Gather(Registry& registry, PlayerNames owner, const Lookups& lookups)
{
	std::vector<const Town*> towns;
	registry.Each<const Town>([&towns, owner](entt::entity, const Town& town) {
		if (town.owner == owner)
		{
			towns.push_back(&town);
		}
	});
	std::ranges::sort(towns, {}, &Town::gained);
	std::vector<physics::temple_heart::Town> lists;
	lists.reserve(towns.size());
	for (const auto* town : towns)
	{
		auto& list = lists.emplace_back();
		for (const auto abode : town->abodes)
		{
			const bool available = registry.Valid(abode);
			const auto* progress = available ? registry.TryGet<const BuildProgress>(abode) : nullptr;
			list.buildings.push_back({.entity = abode,
			                          .available = available,
			                          .life = available ? lookups.lifeOf(abode) : 0.0f,
			                          .built = progress != nullptr ? progress->built : 1.0f,
			                          .field = available && registry.AllOf<Field>(abode),
			                          .footballPitch = available && lookups.footballPitch(abode)});
		}
		for (const auto villager : town->homelessVillagers)
		{
			list.homeless.push_back({.entity = villager, .available = registry.Valid(villager) && !lookups.dying(villager)});
		}
	}
	return lists;
}

} // namespace openblack::ecs::temple_heart_towns
