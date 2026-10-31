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

#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ComputerMindCommon.h"
#include "ComputerMindCreature.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureWatching.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/ComputerMindCreatureRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
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
	minds.GiveUpAction(creature);
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

bool GameHandCreature::HasPlayer(entt::entity creature) const
{
	return IsCreature(creature) &&
	       Locator::entitiesRegistry::value().Get<const Creature>(creature).owner != PlayerNames::NEUTRAL;
}

void GameHandCreature::SetLeashType(entt::entity creature, LeashType type)
{
	if (IsCreature(creature) && Locator::leashSystem::has_value())
	{
		static_cast<void>(Locator::leashSystem::value().ChangeType(creature, type));
	}
}

void GameHandCreature::TakeOffTiedLeash(entt::entity creature)
{
	if (IsCreature(creature) && Locator::leashSystem::has_value() && Locator::leashSystem::value().TiedTo(creature).has_value())
	{
		Locator::leashSystem::value().TakeOff(creature);
	}
}

bool GameHandCreature::TieLeash(entt::entity creature, entt::entity object)
{
	if (!IsCreature(creature) || !Locator::leashSystem::has_value() || !CoordsOfThing(object).has_value())
	{
		return false;
	}
	const bool tied = Locator::leashSystem::value().TieTo(creature, object);
	// The check tools and the scenarios look for this line
	if (const auto logger = spdlog::get("game"))
	{
		SPDLOG_LOGGER_INFO(logger, "Computer god {} ties the leash of creature {} to {}: {}", static_cast<int>(_player),
		                   entt::to_integral(creature), entt::to_integral(object), tied);
	}
	return true;
}

void GameHandCreature::Feedback(entt::entity creature, float feedback)
{
	if (IsCreature(creature) && Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().ReceiveFeedback(creature, feedback);
		if (const auto logger = spdlog::get("game"))
		{
			SPDLOG_LOGGER_INFO(logger, "Computer god {} gives creature {} the feedback {}", static_cast<int>(_player),
			                   entt::to_integral(creature), feedback);
		}
	}
}

float GameHandCreature::DrawFeedbackSeconds()
{
	return Locator::gameRandom::value().GameFloatRand(computer_mind_rules::k_FeedbackSecondsRange);
}

void GameHandCreature::LetGoForCast(MagicType type, const map_coords::MapCoords& hand)
{
	const auto creature = CreatureOf(_player);
	if (creature == entt::null || !Locator::creatureMindSystem::has_value())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& body = registry.Get<const Creature>(creature);
	const auto coords = CoordsOfThing(creature);
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	const auto* tables = Locator::creatureMindSystem::value().GetTables();
	if (!coords.has_value() || mind == nullptr || tables == nullptr)
	{
		return;
	}
	// The times that the creature saw the miracle, and the times that it must see the miracle to learn it. A miracle
	// that the creature cannot learn needs no sightings.
	const auto number = static_cast<size_t>(type);
	float seen = 0.0f;
	float needed = 0.0f;
	if (mind->learnt.has_value() && number < mind->learnt->knowledge.miraclesSeen.size())
	{
		seen = static_cast<float>(mind->learnt->knowledge.miraclesSeen[number].count);
	}
	if (number < tables->miracles.size())
	{
		needed = creature_watching::TimesNeeded(tables->miracles[number].timesToSee,
		                                        creature_mind_tables::MiracleMultiplier(creature::InfoRow(body.species)));
	}
	const float freedom = FreedomOf(_player, creature);
	const float wish = computer_mind_rules::CastLetGoWish(freedom, seen, needed);
	const float distance = gutils::GetDistanceInMetres(hand, *coords);
	if (computer_mind_rules::LetsGoAtCast(freedom, wish, distance, body.size) && Locator::leashSystem::has_value())
	{
		Locator::leashSystem::value().UntieToHand(creature);
	}
}
