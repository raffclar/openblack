/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerDrowning.h"

#include <cstdlib>

#include <spdlog/spdlog.h>

#include "ECS/Components/Animal.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerLastInteraction.h"
#include "ECS/CreatureMimic.h"
#include "ECS/Life.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/VillagerSpeed.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs
{
using namespace components;

namespace
{
/// The villager info's drowningTime: 600 turns for every villager of info.dat
uint16_t DrowningTime(entt::entity villager)
{
	const auto* info = VillagerInfoOf(villager);
	return info != nullptr ? info->drowningTime : static_cast<uint16_t>(600);
}

/// The drowning death: VillagerDead(DEATH_REASON_PLAYER_INTERACTION_DROWN 6, player, 0.01f, 1), player = the player
/// who last dropped the villager, else lastPlayerToInteract; NEUTRAL when there is none. The death itself (states,
/// alignment, counters, soul and skeleton, smoke) is ecs::villager::VillagerDead. openblack keeps no dropper apart from
/// the physics' player, which is what lastPlayerToInteract already holds, so both give the same player here.
void VillagerDeadDrowned(entt::entity villager)
{
	PlayerNames player = PlayerNames::NEUTRAL;
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* last = registry.Valid(villager) ? registry.TryGet<const VillagerLastInteraction>(villager) : nullptr)
	{
		player = last->player;
		registry.RemoveState<VillagerLastInteraction>(villager);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Villager {} drowned (DEATH_REASON_PLAYER_INTERACTION_DROWN, player {})",
	                   static_cast<uint32_t>(villager), static_cast<int>(player));
	villager::VillagerDead(villager, DeathReason::PlayerInteractionDrown, player, 0.01f, 1);
}

LivingAction* ActionOf(entt::entity villager)
{
	return Locator::entitiesRegistry::value().TryGet<LivingAction>(villager);
}

/// stateCounter (a u16 shared by DYING / DEAD / DROWNING / BEING_EATEN) = drowningTime; SetTopState(DROWNING)
void StartDrowning(entt::entity villager)
{
	if (auto* action = ActionOf(villager))
	{
		action->turnsUntilStateChange = DrowningTime(villager);
	}
	SetVillagerState(villager, VillagerStates::Drowning);
}
} // namespace

void RememberLastPlayerToInteract(entt::entity villager, bool byPlayer)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(villager) || !registry.AllOf<Villager>(villager))
	{
		return;
	}
	// the player of the hand that dropped or threw the body. openblack has one hand, the local player PLAYER_ONE
	// (inferred: no other players drop things yet)
	if (byPlayer)
	{
		registry.AssignOrReplaceState<VillagerLastInteraction>(villager, PlayerNames::PLAYER_ONE);
	}
	else
	{
		registry.RemoveState<VillagerLastInteraction>(villager);
	}
}

bool HasSunk(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Villager>(entity))
	{
		// A villager that is not available has not sunk. Then the creature of the player whose hand last dropped it may
		// learn to copy throwing it in the sea
		if (!villager::IsAvailable(entity))
		{
			return false;
		}
		creature_mimic::ConsiderThrownInTheSea(entity);
		// The dead status bit alone (not Living::IsDead) -> SetTopState(14 DYING) and the
		// counter = dyingTimeWithoutGraveyard
		const auto& component = registry.Get<const Villager>(entity);
		if ((component.status & Villager::k_StatusDead) != 0)
		{
			villager::SetTopState(entity, VillagerStates::Dying);
			if (auto* action = ActionOf(entity))
			{
				action->turnsUntilStateChange = static_cast<uint16_t>(villager::InfoOf(entity).dyingTimeWithoutGraveyard);
			}
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: dead villager {} sank: DYING", static_cast<uint32_t>(entity));
			return true;
		}
		StartDrowning(entity);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: villager {} sank: DROWNING for {} turns",
		                   static_cast<uint32_t>(entity), DrowningTime(entity));
		return true;
	}
	if (registry.AllOf<Animal>(entity))
	{
		// An animal: SetDying, SetTopState(LIVING_DEAD 15), ToBeDeleted(0)
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: animal {} sank", static_cast<uint32_t>(entity));
		ToBeDeleted(entity);
		return true;
	}
	return false; // any other object sinks on
}

void VillagerEndPhysicsInWater(entt::entity villager)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.TryGet<const Villager>(villager);
	if (component == nullptr)
	{
		return;
	}
	// Life > 0 -> stateCounter = drowningTime, lastPlayerToInteract = the physics body's player (the hand that dropped
	// or threw it, inherited through what it hit; none without a physics body), SetTopState(DROWNING). Its only reader
	// is Drowning's VillagerDead (TODO(players): openblack has no players to keep there). At 0 life: dead (IsDead)
	// -> SetTopState(14 DYING) and the counter = dyingTimeWithoutGraveyard (never the graveyard's time); else the
	// counter 0 and VillagerDead (the physics' player or the neutral player, 0.01, 1).
	if (life::LifeOf(villager) > 0.0f)
	{
		StartDrowning(villager);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: villager {} at rest in the water: DROWNING for {} turns",
		                   static_cast<uint32_t>(villager), DrowningTime(villager));
		return;
	}
	if (villager::IsDead(villager))
	{
		villager::SetTopState(villager, VillagerStates::Dying);
		if (auto* action = ActionOf(villager))
		{
			action->turnsUntilStateChange = static_cast<uint16_t>(villager::InfoOf(villager).dyingTimeWithoutGraveyard);
		}
		return;
	}
	if (auto* action = ActionOf(villager))
	{
		action->turnsUntilStateChange = 0;
	}
	VillagerDeadDrowned(villager);
}

uint32_t VillagerDrowningState(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	// Called every game turn by the villager's state processing; the info's processChecksEvery only spaces out the
	// periodic checks, and the state waits while an into / out-of clip plays.
	// An INDESTRUCTIBLE villager (SET_INDESTRUCTABLE) has his counter set to 10 first, so he
	// never gets to 0. The u16 counter wraps, like the original's, when it was already 0.
	if (registry.AllOf<Indestructible>(villager))
	{
		action.turnsUntilStateChange = 10;
	}
	--action.turnsUntilStateChange;
	if (action.turnsUntilStateChange % 100 == 0 && std::getenv("OPENBLACK_TEST_SEA") != nullptr)
	{
		const auto* animation = registry.TryGet<const SkeletalAnimation>(villager);
		const auto* transform = registry.TryGet<const Transform>(villager);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Sea test: villager {} drowning, {} turns left, clip {}, y {:.2f}",
		                   static_cast<uint32_t>(villager), action.turnsUntilStateChange,
		                   animation != nullptr ? animation->clipIndex : -1,
		                   transform != nullptr ? transform->position.y : 0.0f);
	}
	if (action.turnsUntilStateChange == 0)
	{
		VillagerDeadDrowned(villager);
	}
	return 1;
}

bool IsDrowning(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return false;
	}
	if (registry.AllOf<Villager>(entity))
	{
		const auto* action = registry.TryGet<const LivingAction>(entity);
		return action != nullptr &&
		       static_cast<VillagerStates>(action->states[static_cast<size_t>(LivingAction::Index::Top)]) ==
		           VillagerStates::Drowning;
	}
	// Any other object: it has a physics body (asleep proxies too; removing the body clears it), and the body's centre
	// of mass is under 0
	const auto* po = physics::PhysicsObjects::Find(entity);
	return po != nullptr && po->body.Centre().y < 0.0f;
}

} // namespace openblack::ecs
