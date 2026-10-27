/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ComputerGameHandCreature.h"

#include "3D/CreatureBody.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureDesires.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/ComputerMindCreatureRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::computer_world;

GameHandCreature::GameHandCreature(PlayerNames player, ComputerPlayer& state)
    : _player(player)
    , _state(state)
{
}

namespace
{

[[nodiscard]] bool IsCreature(entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	return thing != entt::null && registry.Valid(thing) && registry.AllOf<Creature>(thing);
}

} // namespace

bool GameHandCreature::ForceAction(entt::entity creature, uint32_t action, entt::entity object)
{
	if (!IsCreature(creature) || !Locator::creatureMindSystem::has_value())
	{
		return false;
	}
	return Locator::creatureMindSystem::value().ObeyGodOrder(creature, action, object);
}

bool GameHandCreature::PlanForced(entt::entity creature) const
{
	if (!IsCreature(creature))
	{
		return false;
	}
	const auto* mind = Locator::entitiesRegistry::value().TryGet<const CreatureMindState>(creature);
	return mind != nullptr && mind->planForced;
}

void GameHandCreature::ForceDominantDesire(entt::entity creature, int32_t desire, float seconds)
{
	if (!IsCreature(creature) || !Locator::creatureMindSystem::has_value() || desire < 0 ||
	    static_cast<size_t>(desire) >= creature_desires::k_DesireCount)
	{
		return;
	}
	auto& minds = Locator::creatureMindSystem::value();
	minds.AbandonAction(creature);
	minds.MakeDesireDominant(creature, static_cast<creature_desires::Desire>(desire), seconds, false);
}

std::optional<computer_hand::CreatureSpeeds> GameHandCreature::SpeedsOf(entt::entity creature) const
{
	if (!IsCreature(creature) || !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& body = Locator::entitiesRegistry::value().Get<const Creature>(creature);
	const auto& infos = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(body.species);
	if (row >= infos.size())
	{
		return std::nullopt;
	}
	return computer_hand::CreatureSpeeds {.walk = infos.at(row).walkSpeed, .run = infos.at(row).runSpeed};
}

void GameHandCreature::DrawForShortJourney()
{
	static_cast<void>(Locator::gameRandom::value().GameFloatRand(computer_mind_rules::k_ShortJourneyDraw));
}

void GameHandCreature::SetRequiredSpeed(entt::entity creature, float speed)
{
	if (IsCreature(creature) && Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().SetKeptFraction(creature, speed);
	}
}
