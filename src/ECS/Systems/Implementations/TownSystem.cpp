/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TownSystem.h"

#include "ECS/Components/Abode.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

entt::entity TownSystem::FindAbodeWithSpace(entt::entity townEntity) const
{
	const auto& infoConstants = Locator::infoConstants::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto& town = registry.Get<Town>(townEntity);

	entt::entity result = entt::null;
	registry.Each<const Abode>([&town, &infoConstants, &result](entt::entity entity, auto component) {
		if (result != entt::null || component.townId != town.id)
		{
			return;
		}
		const auto& info = infoConstants.abode.at(static_cast<size_t>(component.type));
		if (static_cast<uint32_t>(component.inhabitants.size()) < info.maxVillagersInAbode)
		{
			result = entity;
		}
	});

	return result;
}

void TownSystem::AddHomelessVillagerToTown(entt::entity townEntity, entt::entity villagerEntity)
{
	[[maybe_unused]] auto& registry = Locator::entitiesRegistry::value();
	[[maybe_unused]] auto& registryContext = registry.Context();

	auto& town = registry.Get<Town>(townEntity);
	auto& villager = registry.Get<Villager>(villagerEntity);
	// TODO(bwrsandman): if already assigned to abode or other villager homeless list, remove
	assert(villager.abode == entt::null);
	assert(villager.town == entt::null || villager.town == registryContext.towns[town.id]);
	// the list is ordered, the head first; no caller left (CREATE_VILLAGER_POS uses town_villagers::AddVillagerToTown)
	town.homelessVillagers.insert(town.homelessVillagers.begin(), villagerEntity);
	villager.town = townEntity;
}
