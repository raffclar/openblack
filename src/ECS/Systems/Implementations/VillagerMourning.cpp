/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerMourning.h"

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "Common/GameRandom.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Graveyards.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/VillagerDeathRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerHome.h"
#include "VillagerReactions.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager_mourning = openblack::ecs::villager_mourning;
namespace villager_reactions = openblack::ecs::systems::villager_reactions;

namespace
{
/// Game turns in a second of the game's clock
constexpr auto k_TurnsPerSecond = static_cast<uint32_t>(1000 / ecs::systems::TimeSystemInterface::k_TurnDuration.count());

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

entt::entity EntityOf(LivingAction& action)
{
	return Entities().ToEntity(action);
}

void SetTop(LivingAction& action, VillagerStates state)
{
	Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top, state, false);
}

/// Where the dead lies, while it is there to be mourned
std::optional<glm::vec3> DeadAt(entt::entity villager)
{
	const auto dead = villager_reactions::ReactingTo(villager);
	if (!dead.has_value())
	{
		return std::nullopt;
	}
	const auto* transform = Entities().TryGet<const Transform>(*dead);
	return transform != nullptr ? std::optional(transform->position) : std::nullopt;
}

const ReactionInfo& DeathInfo()
{
	return Locator::infoConstants::value().reaction.at(static_cast<size_t>(Reaction::ReactToDeath));
}

float GameFloatRandom(float range)
{
	return Locator::gameRandom::has_value() ? Locator::gameRandom::value().GameFloatRand(range) : 0.0f;
}
} // namespace

uint8_t villager_mourning::Priority(entt::entity living, entt::entity dead, uint32_t reaction, uint8_t priority)
{
	auto& registry = Entities();
	const bool deadIsCreature = registry.Valid(dead) && registry.AllOf<Creature>(dead);
	const auto* person = registry.TryGet<const Villager>(living);
	const bool graveyard = person != nullptr && graveyards::HasWorkingGraveyard(person->town);
	uint32_t reacting = 0;
	registry.Each<const LivingReaction>([&reacting, reaction](entt::entity, const LivingReaction& state) {
		if (state.reaction == reaction)
		{
			++reacting;
		}
	});
	return villager_death::DeathPriority(deadIsCreature, graveyard, living == dead, reacting, priority);
}

void villager_mourning::Setup(LivingAction& action, entt::entity dead)
{
	auto& registry = Entities();
	const bool deadIsCreature = registry.Valid(dead) && registry.AllOf<Creature>(dead);
	const auto coin = deadIsCreature || !Locator::gameRandom::has_value() ? 0u : Locator::gameRandom::value().GameRand(2);
	switch (villager_death::FirstNoticeOf(deadIsCreature, coin))
	{
	case villager_death::FirstNotice::LookAtCreature:
		SetTop(action, VillagerStates::LookingAtObjectReaction);
		break;
	case villager_death::FirstNotice::Point:
		action.turnsUntilStateChange = 0;
		SetTop(action, VillagerStates::PointAtDeadPerson);
		break;
	case villager_death::FirstNotice::GoTowards:
		SetTop(action, VillagerStates::GoTowardsDeadPerson);
		break;
	}
}

uint32_t villager_mourning::PointAtDeadPerson(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto dead = DeadAt(villager);
	// Facing the dead, it counts the turns it points, and goes to look once they pass a few seconds drawn afresh each turn
	if (!dead.has_value() || villager_reactions::LookAt(villager, *dead))
	{
		++action.turnsUntilStateChange;
		const auto limit = villager_death::PointingLimit(GameFloatRandom(20.0f), k_TurnsPerSecond);
		if (static_cast<int32_t>(action.turnsUntilStateChange) > limit)
		{
			SetTop(action, VillagerStates::GoTowardsDeadPerson);
		}
	}
	return 1;
}

uint32_t villager_mourning::GoTowardsDeadPerson(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto dead = DeadAt(villager);
	if (!dead.has_value())
	{
		return 0;
	}
	const auto here = Entities().Get<const Transform>(villager).position;
	const auto toDead = glm::xz(*dead) - glm::xz(here);
	const float distance = glm::length(toDead);
	const auto approach = villager_death::ApproachToBody(distance, DeathInfo().maxDistanceToRunAwayFromObject);
	if (approach.looks)
	{
		SetTop(action, VillagerStates::LookAtDeadPerson);
		return 1;
	}
	// Straight towards the body, stopping as far off as it keeps from it, then on again from there
	villager_home::SetupMoveTo(action, glm::xz(here) + toDead * (approach.walk / distance),
	                           VillagerStates::GoTowardsDeadPerson);
	return 1;
}

uint32_t villager_mourning::LookAtDeadPerson(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto dead = DeadAt(villager);
	if (!dead.has_value() || villager_reactions::LookAt(villager, *dead))
	{
		action.turnsUntilStateChange = 0;
		SetTop(action, VillagerStates::MournDeadPerson);
	}
	return 1;
}

uint32_t villager_mourning::MournDeadPerson(LivingAction& action)
{
	// It mourns for some seconds, drawn afresh each turn, then goes back to what it was doing
	++action.turnsUntilStateChange;
	const auto limit = villager_death::MourningLimit(GameFloatRandom(3.0f), k_TurnsPerSecond);
	if (static_cast<int32_t>(action.turnsUntilStateChange) > limit)
	{
		villager_reactions::StopReacting(action);
	}
	return 1;
}

bool villager_mourning::ExitMourning(LivingAction& action, VillagerStates next)
{
	// Walking on to another of the reaction's states keeps it too
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	const auto final = Locator::livingActionSystem::value().VillagerGetState(action, LivingAction::Index::Final);
	if (table.at(static_cast<size_t>(next)).isReactionState != 0 ||
	    (final != VillagerStates::InvalidState && table.at(static_cast<size_t>(final)).isReactionState != 0))
	{
		return false;
	}
	if (auto* state = Entities().TryGet<LivingReaction>(EntityOf(action)); state != nullptr)
	{
		state->reaction = 0;
		state->type = Reaction::None;
	}
	return false;
}

bool villager_mourning::ValidateMourning(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto dead = villager_reactions::ReactingTo(villager);
	const bool taken =
	    dead.has_value() && DeathInfo().whetherReactionFinishesIfInitiatorInHand != 0 && Entities().AllOf<InHand>(*dead);
	if (!dead.has_value() || taken)
	{
		villager_reactions::StopReacting(action);
	}
	return true;
}
