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

#include <cstdint>

#include <algorithm>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureActivity.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureLayers.h"
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
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureFriends.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/ReportedHand.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Map.h"
#include "ECS/NearestSearch.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
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
/// The cells that the creature looks in, out from the hand, for a thing to cast the miracle at
constexpr int32_t k_LeashCastCells = 64;

/// The point that the interface of a player picks under the cursor, as the last report of the player's hand tells it.
/// The turn reads the reports and never the pick of the cursor of this computer.
std::optional<glm::vec3> ReportedPickOf(const ecs::Registry& registry, PlayerNames player)
{
	if (!Locator::handSystem::has_value())
	{
		return std::nullopt;
	}
	const auto hands = Locator::handSystem::value().GetHandsOf(player);
	if (!hands.has_value())
	{
		return std::nullopt;
	}
	const auto hand = (*hands)[static_cast<size_t>(HandSystemInterface::Side::Left)];
	const auto* reported = registry.Valid(hand) ? registry.TryGet<const ReportedHand>(hand) : nullptr;
	return reported != nullptr ? std::optional(reported->collidePoint) : std::nullopt;
}
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
	EndReaction(creature);
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

void CreatureMindSystem::CastOnLeashAfterReaction(entt::entity creature, size_t miracle)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	const auto* body = mind != nullptr ? registry.TryGet<const Creature>(creature) : nullptr;
	if (body == nullptr || !mind->learnt.has_value() || !Locator::leashSystem::has_value() ||
	    !Locator::entitiesMap::has_value())
	{
		return;
	}
	// A leash of its player must be on, held or tied. The leash does not have to hold this creature.
	const auto& leashes = Locator::leashSystem::value();
	creature_reaction::LeashCastState state;
	registry.Each<const Creature>([&](entt::entity other, const Creature& otherBody) {
		if (otherBody.owner == body->owner && leashes.IsLeashed(other))
		{
			state.leashOn = true;
			state.tied = state.tied || leashes.TiedTo(other).has_value();
		}
	});
	const auto& known = mind->learnt->knowledge.miraclesKnown;
	state.knowsMiracle = miracle < known.size() && known.at(miracle);
	if (const auto picked = ReportedPickOf(registry, body->owner); picked.has_value())
	{
		state.pickedPoint = glm::vec2(picked->x, picked->z);
	}
	const auto centre = creature_reaction::LeashCastCentre(state);
	if (!centre.has_value())
	{
		return;
	}
	// It finds the first thing other than itself in a spiral of cells out from the point that the interface picked
	const auto& map = Locator::entitiesMap::value();
	const auto thing = ecs::nearest_search::FindFirst(map_coords::FromMetres(*centre), k_LeashCastCells, creature,
	                                                  [&map](glm::ivec2 cell) { return map.GetAllInCell(cell); });
	if (thing == entt::null)
	{
		return;
	}
	// The cast replaces what waits for the end of its work, and the leash leads the reaction now
	mind->followUp = creature_follow_up::FollowUp::CastOnLeash;
	mind->leashCast = CreatureMindState::LeashCast {.thing = thing, .miracle = static_cast<uint32_t>(miracle)};
	mind->ledByLeash = true;
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} on the leash will cast miracle {} at {} after its reaction",
	                    entt::to_integral(creature), miracle, entt::to_integral(thing));
}

void CreatureMindSystem::CastOnLeash(entt::entity creature, CreatureMindState& mind)
{
	const auto cast = std::exchange(mind.leashCast, std::nullopt);
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	if (!cast.has_value() || !registry.Valid(cast->thing) || tables == nullptr || cast->miracle >= tables->miracles.size())
	{
		return;
	}
	const auto action = tables->miracles[cast->miracle].action;
	if (action >= tables->actions.size() || tables->actions[action].desire >= creature_desires::k_DesireCount)
	{
		return;
	}
	// The cast is about the whole thing that the thing is part of. A creature is its own whole thing. With no whole
	// thing, the cast is about the creature itself.
	const auto whole =
	    registry.AllOf<Creature>(cast->thing) ? std::optional(cast->thing) : WholeOfShownThing(creature, mind, cast->thing);
	// The action that its body plays ends at two times the usual speed
	if (Locator::creatureAnimationSystem::has_value())
	{
		Locator::creatureAnimationSystem::value().Hurry(creature);
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} casts miracle {} at {} for the leash", entt::to_integral(creature),
	                    cast->miracle, entt::to_integral(cast->thing));
	static_cast<void>(ForcePlan(creature, {.desire = static_cast<Desire>(tables->actions[action].desire),
	                                       .action = tables->actions[action].name,
	                                       .object = cast->thing,
	                                       .activityObject = whole.value_or(creature)}));
}

entt::entity CreatureMindSystem::FeedSiteAltar(entt::entity flag)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* sign = registry.Valid(flag) ? registry.TryGet<const WorshipSiteFlag>(flag) : nullptr;
	const auto* site = sign != nullptr && registry.Valid(sign->site) ? registry.TryGet<const WorshipSite>(sign->site) : nullptr;
	if (site == nullptr || !registry.Valid(site->altar) || !registry.AllOf<Transform>(site->altar))
	{
		return entt::null;
	}
	return site->altar;
}

void CreatureMindSystem::FeedWorshipSite(entt::entity creature, CreatureMindState& mind)
{
	const auto flag = std::exchange(mind.feedSiteFlag, entt::null);
	const auto altar = FeedSiteAltar(flag);
	if (altar == entt::null)
	{
		return;
	}
	const auto action = creature_follow_up::FeedWorshipSiteAction(
	    KnowsAction(creature, CreatureActionLearningType::Normal, creature_follow_up::k_FishingSkill),
	    KnowsAction(creature, CreatureActionLearningType::Magic, creature_follow_up::k_FoodMiracle));
	if (!action.has_value())
	{
		return;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} feeds the worship site of altar {} for the leash: {}",
	                    entt::to_integral(creature), entt::to_integral(altar), *action);
	// The plan is about the flag, which stands at the place of the altar of its site
	static_cast<void>(ForcePlan(creature, {.desire = Desire::ObeyPlayer, .action = *action, .object = flag, .ordered = true}));
}

void CreatureMindSystem::ReactToNiceMagic(entt::entity creature, const glm::vec3& point, std::optional<size_t> learn)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		return;
	}
	EndReaction(creature);
	const auto random = [this](uint32_t range) { return Random(range); };
	Replan(creature, creature_mind::Activity::Planned,
	       creature_mind::ExamineMiracle({point.x, point.z}, ShownSize(*body) * k_HeightOfSizeOne, random));
	if (learn.has_value())
	{
		WatchMiracle(creature, *learn);
	}
}

void CreatureMindSystem::EndReaction(entt::entity creature)
{
	if (Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::value().StopReacting(creature);
	}
}

void CreatureMindSystem::StartReacting(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr)
	{
		// It keeps the plan that it does now, for all kinds of reaction. After it takes a thing from the hand, it can
		// continue that plan.
		mind->keptPlan = mind->planner.current;
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
		ComeAcross(creature, *mind, thing);
	}
	// A plan done only to a thing of some use drops nothing for one the plan already acts on or one of no use
	const bool wanted =
	    !setup.plan.has_value() || !setup.plan->onlyIfUseful ||
	    (there &&
	     !(mind->planner.current.has_value() && mind->planner.current->object == std::optional(entt::to_integral(thing))) &&
	     ObjectToActOnUsefulness(creature, *mind, setup.plan->desire, thing) > 0.0f);
	if (setup.plan.has_value() && (setup.plan->onSelf || there) && wanted)
	{
		// The plan of a reaction is not a forced plan, so a more urgent reaction can replace it
		ForceActivity(creature, {.desire = setup.plan->desire,
		                         .action = setup.plan->action,
		                         .actionObject = setup.plan->onSelf ? creature : thing,
		                         .forced = false});
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
	ComeAcross(creature, *mind, other);
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
/// The names of the actions in the action table that show a dead creature. Dying for good as a script orders is not
/// one of them: such a creature lies as a dead one does, but it does not count as dead.
constexpr std::string_view k_DieAction = "Die";
constexpr std::string_view k_DeadForeverAction = "DeadForever";

} // namespace

bool ecs::systems::mind_detail::IsDead(const ecs::Registry& registry, entt::entity creature,
                                       const creature_mind_tables::Tables& tables)
{
	// A creature knocked out lies dead until it gets up at home
	if (registry.AllOf<CreatureKnockedOut>(creature))
	{
		return true;
	}
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	if (mind == nullptr || !mind->planActive || !mind->planner.current.has_value() ||
	    mind->planner.current->action >= tables.actions.size())
	{
		return false;
	}
	const auto& name = tables.actions[mind->planner.current->action].name;
	return name == k_DieAction || name == k_DeadForeverAction;
}

bool ecs::systems::mind_detail::AreFriends(const ecs::Registry& registry, entt::entity first, entt::entity second)
{
	const auto friendsOf = [&registry](entt::entity creature) {
		const auto* friends = registry.TryGet<const CreatureFriends>(creature);
		return friends != nullptr ? std::span<const uint32_t>(friends->friends) : std::span<const uint32_t>();
	};
	return creature_activity::AreFriends(friendsOf(first), entt::to_integral(first), friendsOf(second),
	                                     entt::to_integral(second));
}

bool ecs::systems::mind_detail::MayBeAngryWith(const ecs::Registry& registry, entt::entity other, entt::entity creature,
                                               const creature_mind_tables::Tables& tables)
{
	// A creature is never angry with a friend
	if (other == creature || AreFriends(registry, other, creature) || IsDead(registry, other, tables) ||
	    !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& rows = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(registry.Get<const Creature>(other).species);
	return row < rows.size() && rows.at(row).canCreatureAttackMe != 0;
}

namespace
{

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
		return ecs::systems::mind_detail::MayBeAngryWith(registry, other, creature, tables);
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
	// An action that the body started plays on, and a point plays its end
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
	{
		Locator::creatureObjectActionSystem::value().StopForPlan(creature, true);
	}
	Abandon(mind);
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().ReleaseCreatureCast(creature);
	}
	creature_mind::Plan(mind.idle, creature_mind::Activity::None, {});
	creature_learning::MakeFullyDominant(*mind.desires, desire, mind_detail::DesireFloorOf(creature));
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
		ComeAcross(other, *otherMind, creature);
		const auto theirs = DesireAboutCreature(other, *otherMind, creature, {});
		if (!mine.has_value())
		{
			return false;
		}
		const auto worths = [&]() {
			ComeAcross(creature, mind, other);
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
			// When the ages are near, the creature that the game made first is dominant
			return creature_reaction::IsDominant(
			    ownNeeds != nullptr ? ownNeeds->needs.age : 0, otherNeeds != nullptr ? otherNeeds->needs.age : 0,
			    registry.Get<const Creature>(creature).made < registry.Get<const Creature>(other).made);
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
