/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownWorkshops.h"

#include <algorithm>
#include <iterator>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Registry.h"
#include "ECS/WorkshopRules.h"
#include "Locator.h"

namespace openblack::town_workshops
{
using ecs::components::Abode;
using ecs::components::Town;
using ecs::components::Workshop;

void StartsWorking(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(building) || !registry.AllOf<Workshop>(building))
	{
		return;
	}
	const auto* abode = registry.TryGet<const Abode>(building);
	if (abode == nullptr)
	{
		return;
	}
	const auto& towns = registry.Context().towns;
	const auto town = towns.find(abode->townId);
	if (town == towns.end() || !registry.Valid(town->second) || !registry.AllOf<Town>(town->second))
	{
		return;
	}
	ecs::workshop::StartsWorkingInTown(registry.Get<Town>(town->second).workshops, building);
}

std::vector<entt::entity> Of(entt::entity town)
{
	const auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> workshops;
	const auto* data = registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
	if (data == nullptr)
	{
		return workshops;
	}
	std::ranges::copy_if(data->workshops, std::back_inserter(workshops), [&registry](entt::entity workshop) {
		return registry.Valid(workshop) && registry.AllOf<Workshop>(workshop);
	});
	return workshops;
}

} // namespace openblack::town_workshops
