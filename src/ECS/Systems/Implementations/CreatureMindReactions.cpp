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

#include <algorithm>
#include <span>
#include <string_view>
#include <vector>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureMiracleReactions.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreatureReactionRules.h"
#include "Creature/CreatureWatching.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using creature_desires::Desire;

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

namespace
{
/// The names of the actions in the action table that show a dead creature
constexpr std::string_view k_DieAction = "Die";
constexpr std::string_view k_DeadForeverAction = "DeadForever";

/// Whether a creature is dead. A dead creature has a plan with one of these actions.
bool IsDead(const ecs::Registry& registry, entt::entity creature, const creature_mind_tables::Tables& tables)
{
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	if (mind == nullptr || !mind->planActive || !mind->planner.current.has_value() ||
	    mind->planner.current->action >= tables.actions.size())
	{
		return false;
	}
	const auto& name = tables.actions[mind->planner.current->action].name;
	return name == k_DieAction || name == k_DeadForeverAction;
}

/// Whether a creature can be angry with a different creature. The other creature must not be dead. Its species must
/// let creatures attack it.
// TODO(usefulness-use-tree): The game also refuses anger when one creature is in a list of the other creature.
// It has two of these lists. One list contains the creatures that copy the action of the creature. These lists do not
// exist in openblack.
bool MayBeAngryWith(const ecs::Registry& registry, entt::entity other, entt::entity creature,
                    const creature_mind_tables::Tables& tables)
{
	if (other == creature || IsDead(registry, other, tables) || !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& rows = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(registry.Get<const Creature>(other).species);
	return row < rows.size() && rows.at(row).canCreatureAttackMe != 0;
}

/// Whether a desire can be about a different creature:
/// - Anger can, if the creature can be angry with the other creature.
/// - The desire to impress cannot. That desire is about the towns of other players.
/// - Compassion and the desires about friends and creatures can.
bool AppliesToCreature(const ecs::Registry& registry, Desire desire, entt::entity other, entt::entity creature,
                       const creature_mind_tables::Tables& tables)
{
	switch (desire)
	{
	case Desire::Impress:
		return false;
	case Desire::Anger:
		return MayBeAngryWith(registry, other, creature, tables);
	default:
		return true;
	}
}
} // namespace

std::optional<Desire> CreatureMindSystem::DesireAboutCreature(entt::entity creature, const CreatureMindState& mind,
                                                              entt::entity other, std::span<const Desire> leftOut)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	const auto place = BelievedPlace(mind, other);
	const auto* at = registry.TryGet<const Transform>(creature);
	if (tables == nullptr || !mind.desires.has_value() || !mind.learnt.has_value() || !place.has_value() || at == nullptr ||
	    !registry.AllOf<Creature>(other))
	{
		return std::nullopt;
	}
	// TODO(usefulness-use-tree): A script can let a creature attack the town of its player. Then the creature is not
	// angry with a creature of its player. openblack does not keep this script setting yet.
	std::vector<creature_planner::DesireOnThing> weighed;
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		const auto desire = static_cast<Desire>(d);
		const auto& state = mind.desires->desires.at(d);
		if (!state.activated || state.suppressedTurns != 0 || !tables->rules.at(d).aboutSomething ||
		    !AppliesToCreature(registry, desire, other, creature, *tables) ||
		    std::ranges::find(leftOut, desire) != leftOut.end())
		{
			continue;
		}
		const auto nearness = ecs::systems::mind_detail::Nearness(registry, creature, at->position, other, *place,
		                                                          tables->rules.at(d).distanceWeight);
		weighed.push_back({.desire = desire,
		                   .strength = state.value,
		                   .usefulness = ActivityUsefulness(creature, mind, desire, other) * nearness});
	}
	return creature_planner::BestDesireOnThing(weighed);
}

bool CreatureMindSystem::ForceDesireAbout(entt::entity creature, CreatureMindState& mind, Desire desire, entt::entity thing)
{
	if (!mind.desires.has_value())
	{
		return false;
	}
	// The current action stops as a failure. This occurs also when the creature finds no new plan.
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	Abandon(mind);
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().ReleaseCreatureCast(creature);
	}
	creature_mind::Plan(mind.idle, creature_mind::Activity::None, {});
	creature_learning::MakeFullyDominant(*mind.desires, desire);
	mind.leash.obeying = false;
	mind.leash.actOn = {entt::to_integral(thing)};
	mind.plannedTurn = mind.turn + 1;
	PlanCreature(creature, mind, false, desire);
	mind.leash.actOn.clear();
	return mind.planActive && mind.planner.current.has_value() && mind.planner.current->desire == desire;
}

bool CreatureMindSystem::WeighWatchedCreature(entt::entity creature, CreatureMindState& mind, entt::entity other)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* otherMind = registry.Valid(other) ? registry.TryGet<CreatureMindState>(other) : nullptr;
	const auto* tables = GetTables();
	if (otherMind == nullptr || tables == nullptr || !mind.desires.has_value())
	{
		return false;
	}
	std::vector<Desire> leftOut;
	for (;;)
	{
		// Find the desire of the creature about the other creature. The other creature sees this creature, and finds
		// its desire about this creature.
		const auto mine = DesireAboutCreature(creature, mind, other, leftOut);
		ComeAcross(*otherMind, creature);
		const auto theirs = DesireAboutCreature(other, *otherMind, creature, {});
		if (!mine.has_value())
		{
			return false;
		}
		const auto worths = [&]() {
			ComeAcross(mind, other);
			const auto* at = registry.TryGet<const Transform>(creature);
			const auto place = BelievedPlace(mind, other);
			const auto worth = [&](Desire desire) {
				if (at == nullptr || !place.has_value())
				{
					return 0.0f;
				}
				const auto d = static_cast<size_t>(desire);
				return creature_reaction::WorthAbout(
				    mind.desires->desires.at(d).value, ActivityUsefulness(creature, mind, desire, other),
				    ecs::systems::mind_detail::Nearness(registry, creature, at->position, other, *place,
				                                        tables->rules.at(d).distanceWeight));
			};
			return std::pair {worth(Desire::Anger), worth(Desire::Fear)};
		};
		const auto dominant = [&]() {
			const auto* ownNeeds = registry.TryGet<const CreatureNeeds>(creature);
			const auto* otherNeeds = registry.TryGet<const CreatureNeeds>(other);
			// TODO(usefulness-use-tree): When the ages have a difference of ten years or less, the game uses the order
			// of the creatures in its object store. openblack does not have this order. It uses the entity index.
			return creature_reaction::IsDominant(ownNeeds != nullptr ? ownNeeds->needs.age : 0,
			                                     otherNeeds != nullptr ? otherNeeds->needs.age : 0,
			                                     entt::to_entity(other) < entt::to_entity(creature));
		};
		const auto verdict = creature_reaction::WeighWatch(mine, theirs, worths, dominant);
		if (verdict.weighAgainWithoutFear)
		{
			leftOut.push_back(Desire::Fear);
			continue;
		}
		if (!verdict.act.has_value())
		{
			return false;
		}
		[[maybe_unused]] const bool planned = ForceDesireAbout(creature, mind, *verdict.act, other);
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} stops its watch of {} for desire {} ({})",
		                    entt::to_integral(creature), entt::to_integral(other), static_cast<int>(*verdict.act),
		                    planned ? "planned" : "no plan");
		// The creature records the turn when it examined the other creature
		if (auto* belief = mind.beliefs.Find(other); belief != nullptr && Locator::reactionSystem::has_value())
		{
			belief->inspected = Locator::reactionSystem::value().CurrentTurn();
		}
		return true;
	}
}
