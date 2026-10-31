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

#include <algorithm>

#include <glm/vec2.hpp>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerHome.h"
#include "VillagerReactions.h"
#include "VillagerWorship.h"

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
	// This state does not move the villager. The plain walk that it starts moves the villager, and the walk ends in this
	// state again.
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
	const auto stone = ecs::thing_map_place::Of(registry, traveller->stone);
	auto& living = Locator::livingActionSystem::value();
	// Nearer to the stone than one step of its walk, the villager goes into the jump
	if (ecs::villager_home::AreWeThere(villager, glm::ivec2 {stone.x, stone.z}))
	{
		living.VillagerSetState(action, LivingAction::Index::Top, VillagerStates::TeleportReaction, false);
		return 1;
	}
	// Else it starts a walk round things to the stone. The walk ends in the state that the villager works towards.
	const auto goal = registry.Get<const Transform>(traveller->stone).position;
	ecs::villager_home::SetupMoveTo(action, glm::vec2 {goal.x, goal.z}, living.VillagerGetFinalState(action));
	return 1;
}

bool ecs::villager_teleport::LeavesTheWayToWorship(VillagerStates next, bool nextIsFinal)
{
	return nextIsFinal && std::ranges::find(k_TeleportStates, next) == k_TeleportStates.end();
}

bool ecs::villager_teleport::ExitReactToTeleport(LivingAction& action, VillagerStates next)
{
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	if (LeavesTheWayToWorship(next, table.at(static_cast<size_t>(next)).isFinalState != 0))
	{
		ecs::villager_worship::LeaveTheWayToWorshipSite(Locator::entitiesRegistry::value().ToEntity(action));
	}
	return ecs::systems::villager_reactions::ExitReaction(action, next);
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
