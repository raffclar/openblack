/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerEaten.h"

#include "ECS/Components/Indestructible.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/VillagerEatenRules.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"
#include "VillagerFire.h"

using namespace openblack::ecs;
using openblack::ecs::components::Indestructible;
using openblack::ecs::components::LivingAction;

namespace rules = openblack::ecs::villager_eaten_rules;

uint32_t villager_eaten::Downed(LivingAction& action)
{
	// The clip of the fall plays to its end, then the hunter eats the villager for its turns
	Locator::livingActionSystem::value().VillagerPlayAnimThenSetState(action, VillagerStates::BeingEaten);
	action.turnsUntilStateChange = rules::k_BeingEatenTurns;
	return 1;
}

uint32_t villager_eaten::BeingEaten(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	const auto step = rules::BeingEatenTurn(action.turnsUntilStateChange, registry.AllOf<Indestructible>(villager));
	action.turnsUntilStateChange = step.turnsLeft;
	switch (step.result)
	{
	case rules::EatenTurn::Eaten:
		break;
	case rules::EatenTurn::GetsUp:
		Locator::livingActionSystem::value().VillagerPlayAnimThenSetState(action, VillagerStates::Landed);
		break;
	case rules::EatenTurn::Dies:
		// The villager dies as all villagers die. The death goes to its own player, and weighs the life that it had
		villager_fire::DieByEffect(villager, villager_fire::DeathCause {.reason = DeathReason::Animal,
		                                                                .killer = world_objects::PlayerOf(villager),
		                                                                .weight = world_objects::LifeOf(villager)});
		break;
	}
	return 1;
}
