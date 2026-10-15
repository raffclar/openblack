/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <spdlog/spdlog.h>

#include "Creature/CreatureMeals.h"
#include "CreatureMindSystem.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureTallies.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

void CreatureMindSystem::StompedToDeath(entt::entity creature, CreatureMindState& mind, entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing) || ecs::world_objects::LifeOf(thing) > 0.0f)
	{
		return;
	}
	const auto sort = registry.AllOf<Creature>(thing)   ? creature_meals::k_CreatureSort
	                  : registry.AllOf<Villager>(thing) ? creature_meals::k_VillagerSort
	                  : registry.AllOf<Animal>(thing)   ? creature_meals::k_AnimalSort
	                                                    : 0U;
	auto* tallies = registry.TryGet<CreatureTallies>(creature);
	creature_meals::CountKill(tallies != nullptr ? tallies->meals : registry.Assign<CreatureTallies>(creature).meals, sort);
	mind.desireSeenTo = true;
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} stomped {} to death, sort {}", entt::to_integral(creature),
	                    entt::to_integral(thing), sort);
}
