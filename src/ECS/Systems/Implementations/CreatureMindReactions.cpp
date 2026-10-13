/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a creature's mind does about the miracles cast near it: it runs from a nasty one or goes to look at it, goes to
// look at a nice one, and learns them by doing so

#define LOCATOR_IMPLEMENTATIONS

#include <fmt/format.h>

#include "3D/CreatureBody.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureMiracleReactions.h"
#include "Creature/CreatureReactionRules.h"
#include "Creature/CreatureWatching.h"
#include "CreatureMindSystem.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// A creature of size 1 is about this tall
constexpr float k_HeightOfSizeOne = 15.0f;
/// Seeing a nasty miracle adds this much to its fear
constexpr float k_FearOfScaryMagic = 0.5f;
/// Game turns a second
constexpr uint32_t k_TurnsPerSecond = 10;
/// The magic type no creature learns by watching
constexpr size_t k_NotLearnt = static_cast<size_t>(MagicType::LightningBoltPowerUpTwo);
} // namespace

void CreatureMindSystem::WatchMiracle(entt::entity creature, size_t miracle)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	// A mind stilled, as by the freeze spell, learns nothing
	if (tables == nullptr || mind == nullptr || body == nullptr || !mind->learnt.has_value() || mind->paused ||
	    miracle == k_NotLearnt || miracle >= tables->miracles.size())
	{
		return;
	}
	const auto progress = creature_watching::SeeMiracle(
	    mind->learnt->knowledge, miracle, tables->miracles, mind->developmentPhase, mind->turn,
	    mind->leash.miracleSightingWeight, creature_mind_tables::MiracleMultiplier(creature::InfoRow(body->species)));
	if (progress.learnt && !progress.ignored)
	{
		creature_mind_model::Think(*mind->learnt, fmt::format("I've learnt the miracle {}", tables->miracles.at(miracle).name));
	}
}

void CreatureMindSystem::ReactToNastyMagic(entt::entity creature, const glm::vec3& point, std::optional<size_t> learn)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (mind == nullptr || body == nullptr || !mind->desires.has_value())
	{
		return;
	}
	auto& desires = *mind->desires;
	creature_desires::ChangeSource(desires, creature_desires::sources::k_FearFromScaryMagic, k_FearOfScaryMagic);
	// Not afraid, or on the learning leash, it goes to look; otherwise it runs
	const bool onRope = Locator::leashSystem::has_value() && Locator::leashSystem::value().IsLeashed(creature) &&
	                    Locator::leashSystem::value().TypeOf(creature) == LeashType::Rope;
	const bool curious = onRope || !(desires[creature_desires::Desire::Fear].value > 0.0f);
	const float height = ShownSize(*body) * k_HeightOfSizeOne;
	const auto random = [this](uint32_t range) { return Random(range); };
	const glm::vec2 at(point.x, point.z);
	Replan(creature, creature_mind::Activity::Planned,
	       curious ? creature_mind::ExamineMiracle(at, height, random) : creature_mind::RunAwayFromMiracle(at, height, random));
	if (learn.has_value())
	{
		WatchMiracle(creature, *learn);
	}
}

void CreatureMindSystem::ReactToNiceMagic(entt::entity creature, const glm::vec3& point, std::optional<size_t> learn)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		return;
	}
	const auto random = [this](uint32_t range) { return Random(range); };
	Replan(creature, creature_mind::Activity::Planned,
	       creature_mind::ExamineMiracle({point.x, point.z}, ShownSize(*body) * k_HeightOfSizeOne, random));
	if (learn.has_value())
	{
		WatchMiracle(creature, *learn);
	}
}

void CreatureMindSystem::TakeUpReaction(entt::entity creature, entt::entity thing, const creature_reaction::Setup& setup)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr)
	{
		return;
	}
	const bool there = thing != entt::null && registry.Valid(thing);
	if (setup.comesAcross && there)
	{
		ComeAcross(*mind, thing);
	}
	// A plan done only to a thing of some use drops nothing for one the plan already acts on or one of no use
	const bool wanted =
	    !setup.plan.has_value() || !setup.plan->onlyIfUseful ||
	    (there &&
	     !(mind->planner.current.has_value() && mind->planner.current->object == std::optional(entt::to_integral(thing))) &&
	     ObjectToActOnUsefulness(creature, *mind, setup.plan->desire, thing) > 0.0f);
	if (setup.plan.has_value() && (setup.plan->onSelf || there) && wanted)
	{
		ForceActivity(creature, {.desire = setup.plan->desire,
		                         .action = setup.plan->action,
		                         .actionObject = setup.plan->onSelf ? creature : thing});
	}
	// The hand's pick-up sets the watch it was just given to a minute and up to another
	if (setup.longHandWatch)
	{
		const auto seconds = static_cast<float>(Random(game_random::k_FloatRandRange)) *
		                         creature_reaction::k_HandPickUpWatchSeconds * game_random::k_FloatRandScale +
		                     creature_reaction::k_HandPickUpWatchSeconds;
		const auto turns = static_cast<uint32_t>(seconds * static_cast<float>(k_TurnsPerSecond));
		for (auto& step : mind->idle.agenda)
		{
			if (step.kind == creature_mind::Step::Kind::WatchHand)
			{
				step.turns = turns;
			}
		}
	}
	if (setup.skill.has_value())
	{
		WatchSkill(creature, *setup.skill);
	}
	if (setup.desireSource.has_value() && mind->desires.has_value())
	{
		creature_desires::ChangeSource(*mind->desires, setup.desireSource->first, setup.desireSource->second);
	}
}

bool CreatureMindSystem::NoticeCreatureToInspect(entt::entity creature, entt::entity other, uint32_t turn, uint32_t waitTurns)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr || !registry.Valid(other))
	{
		return false;
	}
	ComeAcross(*mind, other);
	auto* belief = mind->beliefs.Find(other);
	if (belief == nullptr || !creature_reaction::InspectDue(belief->inspected, turn, waitTurns))
	{
		return false;
	}
	belief->inspected = turn;
	return true;
}
