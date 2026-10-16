/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The joint actions of two creatures: the asker asks the other creature to be its partner, gives it orders, and makes
// its mind stand still or go free; the joint action stops for both when either creature stops what it does. Also the
// attention that one creature gives another as it sees it.

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureFriendAgendas.h"
#include "Creature/CreatureFriendship.h"
#include "Creature/CreatureHelp.h"
#include "Creature/CreatureKiss.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreatureReactionRules.h"
#include "Creature/CreatureRig.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "Locator.h"
#include "ObjectMeasures.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using creature_desires::Desire;
namespace land = openblack::ecs::creature_exploration_land;

namespace
{
/// The action of the plan that a partner carries out the orders of the asker in
constexpr std::string_view k_ObeyCreatureAction = "ObeyCreature";
/// The actions of dying and of lying dead for good: a creature that does either, or did either last, refuses
constexpr std::string_view k_DieAction = "Die";
constexpr std::string_view k_DeadForeverAction = "DeadForever";
/// The height of a creature of size one
constexpr float k_FriendHeightOfSizeOne = 15.0f;
/// The priority of the plan of a partner that obeys the asker, in the planner's units
constexpr float k_ObeyPriority = 1.0f * creature_planner::k_PriorityScale;

/// The thing that a step uses: what it acts on, where it goes to or what it faces
std::optional<uint32_t> ThingOfStep(const creature_mind::Step& step)
{
	if (step.object.has_value())
	{
		return step.object;
	}
	if (step.movement.object.has_value())
	{
		return step.movement.object;
	}
	return step.order.object;
}

/// The agenda of a creature, and whether it has steps left
bool AgendaRunning(const CreatureMindState& mind)
{
	return mind.idle.step < mind.idle.agenda.size();
}

void Note(CreaturePartner& link, const CreatureMindState& mind)
{
	link.agenda = mind.idle.serial;
	link.agendaRunning = AgendaRunning(mind);
}
} // namespace

bool CreatureMindSystem::RequestPartner(entt::entity asker, entt::entity asked, creature_friendship::RequestKind kind)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	auto* askedMind = registry.Valid(asked) ? registry.TryGet<CreatureMindState>(asked) : nullptr;
	auto* askerMind = registry.Valid(asker) ? registry.TryGet<CreatureMindState>(asker) : nullptr;
	const auto* askedBody = askedMind != nullptr ? registry.TryGet<const Creature>(asked) : nullptr;
	const auto* askerBody = askerMind != nullptr ? registry.TryGet<const Creature>(asker) : nullptr;
	if (tables == nullptr || askedBody == nullptr || askerBody == nullptr || !askedMind->desires.has_value())
	{
		return false;
	}
	// The current plan's action, or the last plan's when it has none now
	const auto action =
	    askedMind->planner.current.has_value() ? askedMind->planner.current->action : askedMind->planner.lastAction;
	const bool dead = action < tables->actions.size() &&
	                  (tables->actions[action].name == k_DieAction || tables->actions[action].name == k_DeadForeverAction);
	const auto* askedLink = registry.TryGet<const CreaturePartner>(asked);
	const auto availability = creature_friendship::AvailabilityOf({
	    .dead = dead,
	    .scriptControlled = registry.AllOf<ScriptControlled>(asked),
	    .askerScriptControlled = registry.AllOf<ScriptControlled>(asker),
	    .partner = askedLink != nullptr ? std::optional(entt::to_integral(askedLink->partner)) : std::nullopt,
	    .asker = entt::to_integral(asker),
	});
	// The asked creature weighs its desires about the asker, as it sees it now
	ComeAcross(asked, *askedMind, asker);
	const auto* at = registry.TryGet<const Transform>(asked);
	const auto place = BelievedPlace(*askedMind, asker);
	const auto worthAbout = [&](Desire desire) {
		if (at == nullptr || !place.has_value())
		{
			return 0.0f;
		}
		const auto d = static_cast<size_t>(desire);
		return creature_reaction::WorthAbout(
		    askedMind->desires->desires.at(d).value, ActivityUsefulness(asked, *askedMind, desire, asker),
		    mind_detail::Nearness(registry, asked, at->position, asker, *place, tables->rules.at(d).distanceWeight));
	};
	const auto answer = creature_friendship::AnswerOf({
	    .kind = kind,
	    .availability = availability,
	    .alwaysAgree = registry.Context().partnersAlwaysAgree,
	    .heightRatio = ShownSize(*askedBody) / std::max(ShownSize(*askerBody), std::numeric_limits<float>::min()),
	    .worthAbout = worthAbout,
	});
	if (!answer.agrees)
	{
		// The asked creature tells its player that it refused
		if (answer.explanation.has_value() && Locator::creatureHelpSystem::has_value())
		{
			Locator::creatureHelpSystem::value().Raise(
			    asked, {.kind = creature_help::Kind::FailureExplanation, .parameter = *answer.explanation});
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} stops because creature {} refused its request {}",
		                    entt::to_integral(asker), entt::to_integral(asked), static_cast<int>(kind));
		return false;
	}
	auto& askerLink = registry.AssignOrReplace<CreaturePartner>(asker, CreaturePartner {.partner = asked});
	Note(askerLink, *askerMind);
	auto& otherLink = registry.AssignOrReplace<CreaturePartner>(asked, CreaturePartner {.partner = asker});
	Note(otherLink, *askedMind);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} is the partner of creature {}", entt::to_integral(asked),
	                    entt::to_integral(asker));
	return true;
}

void CreatureMindSystem::TakeOrder(entt::entity partner, entt::entity asker, const creature_mind::Step& ordered)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(partner) ? registry.TryGet<CreatureMindState>(partner) : nullptr;
	auto* link = registry.TryGet<CreaturePartner>(partner);
	const auto* tables = GetTables();
	if (mind == nullptr || link == nullptr || link->partner != asker || tables == nullptr || !mind->desires.has_value())
	{
		return;
	}
	// It stops what it does, without the stop of the joint action: it does the order, then waits for ever, watching the
	// asker
	EndReaction(partner);
	const auto self = entt::to_integral(asker);
	std::vector agenda {
	    ordered,
	    creature_mind::Step {.kind = creature_mind::Step::Kind::Wait,
	                         .seconds = std::numeric_limits<float>::infinity(),
	                         .gaze = creature_mind::Gaze {.object = self}},
	};
	if (!Replan(partner, creature_mind::Activity::Told, std::move(agenda)))
	{
		return;
	}
	const auto action = creature_mind_tables::FindAction(*tables, k_ObeyCreatureAction);
	if (action.has_value())
	{
		mind->planner.current = creature_planner::Plan {
		    .desire = Desire::ObeyCreature,
		    .action = *action,
		    .object = self,
		    .priority = k_ObeyPriority,
		    .activityObject = self,
		};
		mind->planActive = true;
		mind->planSerial = mind->idle.serial;
		mind->agendaSeen = mind->idle.serial;
		mind->satisfiedByEffect = false;
		mind->desireSeenTo = false;
		mind->planActions = tables->desireActions.at(static_cast<size_t>(Desire::ObeyCreature));
		// It is remembered for the player's feedback as an order, unless nothing says why it wants to obey
		if (const auto source = creature_learning::PlanSource((*mind->desires)[Desire::ObeyCreature]); source.has_value())
		{
			const auto& info = tables->actions[*action];
			RememberAction(partner, *mind,
			               {
			                   .action = *action,
			                   .desire = Desire::ObeyCreature,
			                   .object = self,
			                   .belief = mind_detail::ActedOnBeliefOf(registry, asker, partner),
			                   .activity = self,
			                   .activityBelief = mind_detail::BeliefOf(registry, asker, partner),
			                   .learnable = info.learnable,
			                   .windowSeconds = info.learningWindowSeconds,
			                   .source = *source,
			                   .ordered = true,
			               });
		}
	}
	// The new agenda is the joint action's own, so the joint action goes on
	Note(*link, *mind);
}

void CreatureMindSystem::SetDemobilised(entt::entity partner, bool demobilised)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(partner))
	{
		return;
	}
	if (demobilised)
	{
		registry.AssignOrReplace<CreatureDemobilised>(partner);
	}
	else
	{
		registry.Remove<CreatureDemobilised>(partner);
	}
}

void CreatureMindSystem::EndJointAction(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* link = registry.TryGet<const CreaturePartner>(creature);
	if (link == nullptr)
	{
		return;
	}
	const auto partner = link->partner;
	registry.Remove<CreaturePartner>(creature);
	registry.Remove<CreatureDemobilised>(creature);
	const auto* other = registry.Valid(partner) ? registry.TryGet<const CreaturePartner>(partner) : nullptr;
	if (other == nullptr || other->partner != creature)
	{
		return;
	}
	const auto partnerAgenda = other->agenda;
	registry.Remove<CreaturePartner>(partner);
	registry.Remove<CreatureDemobilised>(partner);
	// The partner stops what it does for the joint action as a failure, and decides afresh. An agenda that it took up
	// of its own accord goes on.
	auto* mind = registry.TryGet<CreatureMindState>(partner);
	if (mind == nullptr || mind->idle.serial != partnerAgenda)
	{
		return;
	}
	AbandonAction(partner);
	if (AgendaRunning(*mind))
	{
		creature_mind::Plan(mind->idle, creature_mind::Activity::None, {});
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} stops because its partner {} told it to", entt::to_integral(partner),
	                    entt::to_integral(creature));
}

void CreatureMindSystem::KeepJointActions()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> ended;
	registry.Each<const CreaturePartner>([&](entt::entity creature, const CreaturePartner& link) {
		const auto* mind = registry.TryGet<const CreatureMindState>(creature);
		// The joint action stops when the creature took up another agenda, or its agenda came to an end
		if (mind == nullptr || !registry.Valid(link.partner) || mind->idle.serial != link.agenda ||
		    (link.agendaRunning && !AgendaRunning(*mind)))
		{
			ended.push_back(creature);
		}
	});
	for (const auto creature : ended)
	{
		EndJointAction(creature);
	}
}

void CreatureMindSystem::NoteAttention(entt::entity creature, CreatureMindState& mind, entt::entity other)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* otherMind = registry.Valid(other) ? registry.TryGet<const CreatureMindState>(other) : nullptr;
	if (otherMind == nullptr || !mind.learnt.has_value())
	{
		return;
	}
	// The other creature's agenda uses what it believes about this creature. With no belief about this creature, a
	// plan or a step that uses nothing counts, as in the game.
	creature_friendship::AgendaUse use {.running = otherMind->planActive && AgendaRunning(*otherMind)};
	if (otherMind->planner.current.has_value())
	{
		const auto& plan = *otherMind->planner.current;
		use.plan = {plan.activityObject, plan.object, plan.instrument};
	}
	for (size_t i = otherMind->idle.step; i < otherMind->idle.agenda.size(); ++i)
	{
		use.steps.push_back(ThingOfStep(otherMind->idle.agenda[i]));
	}
	const auto believed =
	    otherMind->beliefs.Find(creature) != nullptr ? std::optional(entt::to_integral(creature)) : std::nullopt;
	if (creature_friendship::AttentionRises(registry.AllOf<ScriptControlled>(creature), creature == other,
	                                        creature_friendship::AgendaUses(use, believed)))
	{
		creature_friendship::ChangeAttention(creature_learning::AttitudeTo(mind.learnt->creatures, entt::to_integral(other)),
		                                     creature_friendship::k_AttentionStep);
	}
}

namespace
{
/// The field nearest the creature, at any distance, that the creature believes in
std::optional<creature_plan_actions::Situation::FoodPile> NearestBelievedField(const ecs::Registry& registry,
                                                                               entt::entity creature, glm::vec3 from)
{
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return std::nullopt;
	}
	std::optional<creature_plan_actions::Situation::FoodPile> nearest;
	float nearestDistance = std::numeric_limits<float>::max();
	for (const auto& belief : mind->beliefs.InListOrder())
	{
		const auto thing = belief.thing;
		const auto* at =
		    registry.Valid(thing) && registry.AllOf<Field>(thing) ? registry.TryGet<const Transform>(thing) : nullptr;
		if (at == nullptr)
		{
			continue;
		}
		const float distance =
		    gutils::GetDistanceInMetres(glm::vec2(from.x, from.z), glm::vec2(at->position.x, at->position.z));
		if (distance < nearestDistance)
		{
			nearestDistance = distance;
			nearest = {.pile = entt::to_integral(thing), .point = glm::vec2(at->position.x, at->position.z)};
		}
	}
	return nearest;
}

/// Where the head bone of the creature is in the world, as its body is posed now
std::optional<glm::vec3> HeadOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* self = registry.TryGet<const Creature>(creature);
	const auto* body = registry.TryGet<const CreatureAnimation>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	if (self == nullptr || body == nullptr || transform == nullptr || !rigs.Contains(creature::GetRigId(self->species)))
	{
		return std::nullopt;
	}
	const auto& points = rigs.Handle(creature::GetRigId(self->species))->actionPoints;
	if (!points.has_value())
	{
		return std::nullopt;
	}
	const auto placement = creature::PlacementMatrix(transform->position, transform->rotation, transform->scale);
	return glm::vec3(creature::PosedBone(points->head, body->boneMatrices, placement)[3]);
}
} // namespace

void CreatureMindSystem::SituateFriends(entt::entity creature, creature_plan_actions::Situation& situation)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (mind == nullptr || transform == nullptr || !mind->desires.has_value())
	{
		return;
	}
	const auto& desires = *mind->desires;
	situation.water = desires.desires.at(static_cast<size_t>(Desire::Water)).value;
	situation.pooDesire = desires.desires.at(static_cast<size_t>(Desire::Poo)).value;
	situation.tiredness = desires.desires.at(static_cast<size_t>(Desire::Tiredness)).value;
	const auto sadDesire = creature_desires::DesireOfSource(desires, creature_desires::sources::k_Sadness);
	situation.sadness = sadDesire.has_value()
	                        ? creature_desires::SourceValue(desires.desires.at(static_cast<size_t>(*sadDesire)),
	                                                        creature_desires::sources::k_Sadness)
	                              .value_or(0.0f)
	                        : 0.0f;
	// The nearest town at any distance, when the creature believes something about it. It wants to attack it when the
	// town is more useful to its anger than to its compassion.
	const auto town = mind_detail::NearestTown(registry, transform->position);
	if (town == entt::null || mind->beliefs.Find(town) == nullptr)
	{
		return;
	}
	situation.attackNearestTown = ActivityUsefulness(creature, *mind, Desire::Compassion, town) <
	                              ActivityUsefulness(creature, *mind, Desire::Anger, town);
}

void CreatureMindSystem::SituateFriend(entt::entity creature, const creature_plan_actions::Executor& executor,
                                       entt::entity friendly, creature_plan_actions::Situation& situation)
{
	using creature_plan_actions::Build;
	switch (executor.build)
	{
	case Build::BeHappyWithFriend:
	case Build::PooWithFriend:
	case Build::SitWithFriend:
	case Build::SleepWithFriend:
	case Build::DrinkWithFriend:
	case Build::HelpTownWithFriend:
	case Build::AttackTownWithFriend:
	case Build::KissFriend:
	case Build::TalkToFriend:
	case Build::TellFriendAJoke:
	case Build::DanceWithFriend:
	case Build::ArgueWithFriend:
	case Build::MopeAboutWithFriend:
	case Build::ShowOffToFriend:
	case Build::GetAttentionFromFriend:
	case Build::TellCreatureToSodOff:
	case Build::ExamineOtherCreatureWithFriend:
	case Build::SitOnTopOfHillWithFriend:
	case Build::EatFromFieldWithFriend:
	case Build::PlayThrowingGameWithFriend:
	case Build::GiveFriendToy:
	case Build::ShowFriendCitadel:
		break;
	default:
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.Valid(friendly) ? registry.TryGet<const Creature>(friendly) : nullptr;
	const auto* at = body != nullptr ? registry.TryGet<const Transform>(friendly) : nullptr;
	const auto* self = registry.TryGet<const Transform>(creature);
	if (at == nullptr || self == nullptr)
	{
		return;
	}
	situation.friendly = creature_plan_actions::Situation::Friend {
	    .point = glm::vec2(at->position.x, at->position.z),
	    .height = k_FriendHeightOfSizeOne * ShownSize(*body),
	    .position = at->position,
	    .radius = object_measures::TwoDRadius(registry, friendly),
	};
	const auto selfPoint = glm::vec2(self->position.x, self->position.z);
	// It sees the friend when the friend is near, or near for the height of the friend
	situation.seesFriend = creature_mind::CanSee(glm::distance(self->position, at->position), situation.friendly->height);
	switch (executor.build)
	{
	case Build::SitWithFriend:
		// A clear place about the point halfway between the two, with nothing standing in it
		situation.clearArea =
		    mind_detail::ClearAreaFor(registry, creature, selfPoint + (situation.friendly->point - selfPoint) * 0.5f,
		                              creature_mind::SitTogetherRadius(situation.height, situation.friendly->height), false);
		break;
	case Build::SleepWithFriend:
		// A clear place about the creature itself
		situation.clearArea =
		    mind_detail::ClearAreaFor(registry, creature, selfPoint,
		                              creature_mind::SleepTogetherRadius(situation.height, situation.friendly->height), false);
		break;
	case Build::DrinkWithFriend:
		// The coast nearest the creature, and the sea nearest that coast, by its map of the land
		situation.explorePlace = land::FindPlace(creature, creature_exploration::Kind::Coast, selfPoint, false);
		situation.exploreSea = situation.explorePlace.has_value()
		                           ? land::FindPlace(creature, creature_exploration::Kind::Sea, *situation.explorePlace, false)
		                           : std::nullopt;
		break;
	case Build::KissFriend:
		// Each kisser stands the mean of the two kissing distances from the point halfway between them
		if (Locator::creatureAnimationSystem::has_value())
		{
			auto& bodies = Locator::creatureAnimationSystem::value();
			const auto lips = bodies.KissPointsOf(creature);
			const auto friendLips = bodies.KissPointsOf(friendly);
			if (lips.has_value() && friendLips.has_value())
			{
				situation.kissingDistance = creature_kiss::MeanKissingDistance(*lips, *friendLips);
				SPDLOG_LOGGER_DEBUG(
				    spdlog::get("game"),
				    "Creature {} kisses creature {} from {:.2f} each side (lips {:.2f} {:.2f} {:.2f} and {:.2f} "
				    "{:.2f} {:.2f})",
				    entt::to_integral(creature), entt::to_integral(friendly), *situation.kissingDistance, lips->high.y,
				    lips->low.y, lips->high.z, friendLips->high.y, friendLips->low.y, friendLips->high.z);
			}
		}
		break;
	case Build::SitOnTopOfHillWithFriend:
	{
		// A clear place on the hill nearest the creature, by its map of the land, big enough for the two
		const auto hill = land::FindPlace(creature, creature_exploration::Kind::Hill, selfPoint, false);
		situation.clearArea = hill.has_value()
		                          ? mind_detail::ClearAreaFor(
		                                registry, creature, *hill,
		                                creature_mind::HillTogetherRadius(situation.radius, situation.friendly->radius), false)
		                          : std::nullopt;
		break;
	}
	case Build::EatFromFieldWithFriend:
		situation.friendsField = NearestBelievedField(registry, creature, self->position);
		break;
	case Build::ShowOffToFriend:
		if (situation.home.has_value())
		{
			const float ground = Locator::terrainSystem::has_value()
			                         ? Locator::terrainSystem::value().GetHeightAt(*situation.home)
			                         : self->position.y;
			situation.homePlace = glm::vec3(situation.home->x, ground, situation.home->y);
		}
		break;
	case Build::ShowFriendCitadel:
		situation.head = HeadOf(registry, creature);
		break;
	case Build::HelpTownWithFriend:
	case Build::AttackTownWithFriend:
	{
		// The nearest town at any distance, with a storage pit, when the creature believes something about the town
		const auto* mind = registry.TryGet<const CreatureMindState>(creature);
		const auto town = mind_detail::NearestTown(registry, self->position);
		const auto pit = town != entt::null ? ecs::construction::StoragePitOf(town) : std::nullopt;
		const auto* townAt = town != entt::null ? registry.TryGet<const Transform>(town) : nullptr;
		if (mind == nullptr || !pit.has_value() || townAt == nullptr || mind->beliefs.Find(town) == nullptr)
		{
			break;
		}
		situation.friendsTown = creature_mind::FriendsTown {
		    .town = entt::to_integral(town),
		    .pit = entt::to_integral(*pit),
		    .townPoint = glm::vec2(townAt->position.x, townAt->position.z),
		    .pitRadius = object_measures::TwoDRadius(registry, *pit),
		};
		break;
	}
	default:
		break;
	}
}
