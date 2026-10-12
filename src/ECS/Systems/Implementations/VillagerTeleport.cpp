/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerTeleport.h"

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/WallHugRules.h"
#include "Locator.h"
#include "Magic/TeleportRules.h"
#include "VillagerHome.h"
#include "VillagerReactions.h"

using namespace openblack;
using namespace openblack::ecs::components;

void ecs::villager_teleport::StopReacting(LivingAction& action, entt::entity villager)
{
	Locator::entitiesRegistry::value().Remove<TeleportTraveller>(villager);
	ecs::systems::villager_reactions::StopReacting(action);
}

uint32_t ecs::villager_teleport::GoTowardsTeleportReaction(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	auto* traveller = registry.TryGet<TeleportTraveller>(villager);
	if (traveller == nullptr)
	{
		Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top, VillagerStates::DecideWhatToDo,
		                                                      false);
		return 1;
	}
	if (!registry.Valid(traveller->stone) || !registry.AllOf<Transform>(traveller->stone))
	{
		StopReacting(action, villager);
		return 1;
	}
	const auto stone = registry.Get<const Transform>(traveller->stone).position;
	const auto here = registry.Get<const Transform>(villager).position;
	const auto* wallHug = registry.TryGet<const WallHug>(villager);
	const float step = wallHug != nullptr ? ecs::wall_hug::StepMetres(wallHug->speed) : 0.0f;
	auto& living = Locator::livingActionSystem::value();
	// Within a step of the stone it is there, tested afresh each turn
	if (magic::teleport::WithinAStep(here, stone, step))
	{
		living.VillagerSetState(action, LivingAction::Index::Top, VillagerStates::TeleportReaction, false);
		return 1;
	}
	// Otherwise its walk to the stone starts afresh, still in this state, to end in what it was to do
	ecs::villager_home::SetupMobileMoveTo(action, {stone.x, stone.z});
	return 1;
}

uint32_t ecs::villager_teleport::TeleportReaction(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	const auto* traveller = registry.TryGet<const TeleportTraveller>(villager);
	if (traveller == nullptr)
	{
		Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top, VillagerStates::DecideWhatToDo,
		                                                      false);
		return 1;
	}
	if (Locator::teleportSystem::has_value())
	{
		Locator::teleportSystem::value().DoTeleport(traveller->stone, villager, false);
	}
	StopReacting(action, villager);
	return 1;
}
