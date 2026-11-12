/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Plans that the creature does not choose itself: plans that a script, a gift or an event forces on it

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Animals/AnimalRules.h"
#include "Camera/Camera.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Common/X87Vector.h"
#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureActionHistory.h"
#include "Creature/CreatureActivity.h"
#include "Creature/CreatureAgendaArea.h"
#include "Creature/CreatureAttention.h"
#include "Creature/CreatureBringHome.h"
#include "Creature/CreatureDance.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesireSources.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureExplorationMap.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureGiving.h"
#include "Creature/CreatureKindAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMeals.h"
#include "Creature/CreatureMind.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureMode.h"
#include "Creature/CreatureMusicReaction.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreaturePlayerAttention.h"
#include "Creature/CreatureRestAndHome.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureSacrifice.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/CreatureScriptPlay.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureTownCompassion.h"
#include "Creature/CreatureTownTheft.h"
#include "Creature/CreatureTraining.h"
#include "Creature/CreatureTurnOrder.h"
#include "Creature/CreatureWatching.h"
#include "Creature/LeashOrders.h"
#include "Creature/LeashRules.h"
#include "CreatureMindFootball.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "CreatureMindTownHelp.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LandForest.h"
#include "ECS/Components/LearningMarker.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicPile.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureBodyFile.h"
#include "ECS/CreatureCarried.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreatureNaming.h"
#include "ECS/FixedKinds.h"
#include "ECS/LearningMarker.h"
#include "ECS/PlayerTowns.h"
#include "ECS/Registry.h"
#include "ECS/ReportedCamera.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MusicMoodSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/TempleHeartOf.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/TownDesire.h"
#include "ECS/WallHugRules.h"
#include "ECS/WorldObjects.h"
#include "Graphics/Moon.h"
#include "Gui/TextDatabase.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "MagicLiving.h"
#include "ObjectMeasures.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/MusicMood.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems::mind_detail;
using creature_desires::Desire;
using creature_mind_model::TreeKind;
using creature_plan_actions::Target;

namespace
{
constexpr float k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
constexpr float k_TurnsPerSecond = 1.0f / k_TurnSeconds;
/// The action of the game's table that shows a creature that lies dead after a faint
constexpr std::string_view k_DeathAction = "Die";

constexpr float k_HeightOfSizeOne = 15.0f;
} // namespace

void CreatureMindSystem::AteMagicMushroom(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	// Every magic mushroom it eats counts, also while it is high
	if (auto* body = registry.Valid(creature) ? registry.TryGet<Creature>(creature) : nullptr)
	{
		++body->magicMushroomsEaten;
	}
	if (auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr)
	{
		mind->followUp = creature_follow_up::AfterMushroom(mind->followUp);
	}
}

void CreatureMindSystem::DoFollowUp(entt::entity creature, CreatureMindState& mind, creature_follow_up::FollowUp followUp)
{
	using creature_follow_up::FollowUp;
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} does what waited: {}", entt::to_integral(creature),
	                    static_cast<int>(followUp));
	switch (followUp)
	{
	case FollowUp::BecomeHigh:
		BecomeHigh(creature);
		return;
	case FollowUp::ShowHappy:
		static_cast<void>(ForcePlan(creature, {.desire = Desire::ManifestState,
		                                       .action = "CommunicateState",
		                                       .object = creature,
		                                       .agenda = creature_follow_up::ShowHappy(),
		                                       .ordered = true,
		                                       .fightBodyEndsAgain = true}));
		return;
	case FollowUp::FrightenedOfPlayer:
		static_cast<void>(ForcePlan(creature, {.desire = Desire::Fear,
		                                       .action = creature_follow_up::FrightenedAction(Random(3)),
		                                       .object = creature,
		                                       .ordered = true,
		                                       .fightBodyEndsAgain = true}));
		return;
	case FollowUp::DoLessonAgain:
		DoLessonAgain(creature, mind);
		return;
	case FollowUp::WalkOn:
		WalkOn(creature, mind);
		return;
	case FollowUp::ScratchAfterNewMusic:
		static_cast<void>(ForcePlan(creature, {.desire = Desire::Curiosity,
		                                       .action = "Scratch",
		                                       .object = creature,
		                                       .ordered = true,
		                                       .fightBodyEndsAgain = true}));
		return;
	case FollowUp::DanceAfterLikedMusic:
		DanceAfterMusic(creature, mind);
		return;
	case FollowUp::CastOnLeash:
		CastOnLeash(creature, mind);
		return;
	case FollowUp::FeedWorshipSite:
		FeedWorshipSite(creature, mind);
		return;
	case FollowUp::ShowSlapFeeling:
	{
		const creature_follow_up::Feelings feelings {
		    .compassion = mind.desires.has_value() ? (*mind.desires)[Desire::Compassion].value : 0.0f,
		    .anger = mind.desires.has_value() ? (*mind.desires)[Desire::Anger].value : 0.0f,
		};
		auto agenda = creature_follow_up::ShowSlapFeeling(
		    feelings, [this](uint32_t range) { return Random(range); }, _random.GameFloat(),
		    static_cast<uint32_t>(std::lround(k_TurnsPerSecond)));
		static_cast<void>(ForcePlan(creature, {.desire = Desire::ManifestState,
		                                       .action = "CommunicateState",
		                                       .object = creature,
		                                       .agenda = std::move(agenda),
		                                       .ordered = true,
		                                       .fightBodyEndsAgain = true}));
		return;
	}
	}
}

void CreatureMindSystem::BecomeHigh(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (transform == nullptr)
	{
		return;
	}
	const glm::vec2 position {transform->position.x, transform->position.z};
	const auto radius = object_measures::TwoDRadius(registry, creature);
	const auto findSeat = [&]() -> std::optional<creature_mind::HighSeat> {
		const auto place = ClearAreaFor(registry, creature, position, creature_mind::k_HighSeatRadii * radius, false);
		return place.has_value() ? std::optional(creature_mind::HighSeat {.place = *place, .radius = radius}) : std::nullopt;
	};
	auto agenda = creature_mind::BecomeHigh(findSeat, [this](uint32_t range) { return Random(range); }, _random.GameFloat());
	static_cast<void>(ForcePlan(creature, {.desire = Desire::GetHigh,
	                                       .action = "GetHigh",
	                                       .object = creature,
	                                       .agenda = std::move(agenda),
	                                       .fightBodyEndsAgain = true}));
}

bool CreatureMindSystem::ForcePlan(entt::entity creature, const ForcedPlan& plan)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	const auto* transform = registry.Valid(creature) ? registry.TryGet<const Transform>(creature) : nullptr;
	const auto* tables = GetTables();
	if (mind == nullptr || body == nullptr || transform == nullptr || tables == nullptr)
	{
		return false;
	}
	// A forced plan first stops the reaction that the creature has. What the creature does stops as a failure. This also
	// stops a fight that the creature is in, out of its duel.
	EndReaction(creature);
	// A forced plan takes the place of a faint: a knocked-out creature gets up where it lies
	if (Locator::creatureFightSystem::has_value())
	{
		Locator::creatureFightSystem::value().GetUpForPlan(creature);
	}
	if (!plan.ownFight)
	{
		EndFightForPlan(creature, plan.fightBodyEndsAgain);
	}
	// A creature told to do something as soon as it is made has a mind ready for it
	SetUpMind(creature, *body, *mind);
	const glm::vec2 position {transform->position.x, transform->position.z};
	const auto height = k_HeightOfSizeOne * ShownSize(*body);
	const auto random = [this](uint32_t range) { return Random(range); };
	auto activity = plan.activity.value_or(creature_mind::Activity::Told);
	auto agenda = plan.agenda;
	if (!agenda.has_value())
	{
		if (plan.action == creature_leash_orders::k_HoldAction && plan.object.has_value())
		{
			// It plays with what it is told to hold only the first times that it holds a thing of that sort. A thing
			// with no sort gets no play.
			std::optional<uint32_t> playWith;
			if (const auto sort = mind_detail::ObjectSortOf(registry, *plan.object); sort.has_value())
			{
				auto& held = mind->heldSortCounts[*sort];
				if (held < creature_leash_orders::k_HoldPlayLimit)
				{
					playWith = Random(static_cast<uint32_t>(creature_object_actions::k_KeepAnimationCount));
					++held;
				}
			}
			const bool alreadyHeld = Locator::creatureObjectActionSystem::has_value() &&
			                         Locator::creatureObjectActionSystem::value().GetCarried(creature) == plan.object;
			const auto camera = openblack::ecs::ReportedCameraOfItsPlayer(creature);
			const auto eye = camera.has_value() ? camera->origin : transform->position;
			agenda = creature_leash_orders::Hold(
			    entt::to_integral(*plan.object), alreadyHeld, glm::vec2(eye.x, eye.z), playWith,
			    creature_leash_orders::HoldSeconds(_random.GameFloatRand(creature_leash_orders::k_HoldExtraSeconds),
			                                       k_TurnsPerSecond));
		}
		else if (plan.action == creature_leash_orders::k_ReflectionAction)
		{
			// The sea and the coast that are nearest to the creature on its map of the land. Where the map has no such
			// place, the creature uses the low corner of the map, as in the game.
			namespace land = openblack::ecs::creature_exploration_land;
			constexpr glm::vec2 k_Corner {0.0f};
			const auto sea = land::FindPlace(creature, creature_exploration::Kind::Sea, position, false).value_or(k_Corner);
			const auto coast = land::FindPlace(creature, creature_exploration::Kind::Coast, position, false).value_or(k_Corner);
			agenda = creature_leash_orders::LookAtReflection(sea, coast, height, k_TurnsPerSecond);
		}
		else if (plan.action == creature_leash_orders::k_SleepAction && plan.point.has_value())
		{
			activity = creature_mind::Activity::Sleep;
			agenda = creature_leash_orders::SleepAtHome(*plan.point, height, creature_mind::Sleep(random));
		}
		else if (const auto* executor = creature_plan_actions::For(plan.action))
		{
			creature_plan_actions::Situation situation;
			if (executor->build == creature_plan_actions::Build::Drink)
			{
				situation.drink = DrinkPlaceOf(creature);
			}
			// Told to fish, to give fish to a store, to take fish home or to put fish by a worship site, it fishes at the fish
			// farm nearest to itself, as when it chooses to fish
			if (creature_plan_actions::FishesFirst(*executor))
			{
				situation.fishing = FishingFor(registry, creature, position);
			}
			Situate(creature, situation);
			if (plan.instrument.has_value())
			{
				situation.instrument = entt::to_integral(*plan.instrument);
				situation.instrumentPoint = PointOf(registry, *plan.instrument).value_or(position);
				situation.instrumentRadius = object_measures::TwoDRadius(registry, *plan.instrument);
				situation.usedOneShot = OneShotOf(registry, *plan.instrument);
			}
			situation.handFull = ecs::creature_carried::CarriedBy(registry, creature).has_value();
			if (plan.activityObject.has_value())
			{
				situation.activity = entt::to_integral(*plan.activityObject);
			}
			if (plan.object.has_value() && registry.Valid(*plan.object))
			{
				situation.thingHeight = object_measures::Height(registry, *plan.object);
				situation.thingRadius = object_measures::TwoDRadius(registry, *plan.object);
				situation.thingLiving =
				    Locator::creatureObjectActionSystem::has_value() &&
				    creature_object_actions::IsLiving(Locator::creatureObjectActionSystem::value().KindOf(*plan.object).thing);
				if (executor->build == creature_plan_actions::Build::EatFromStoragePit)
				{
					SituateStoreFood(registry, *plan.object, situation);
				}
			}
			if (const auto home = ecs::creature_home::HomeOf(registry, creature))
			{
				situation.home = glm::vec2(home->x, home->z);
			}
			const auto point =
			    plan.object.has_value() ? PointOf(registry, *plan.object).value_or(position) : plan.point.value_or(position);
			// Sleeping by a thing, it looks for a clear place four times as wide as it is tall about it; one not under a
			// script's control also wants nothing living there
			if (executor->build == creature_plan_actions::Build::SleepByObject)
			{
				situation.clearArea =
				    ClearAreaFor(registry, creature, point, creature_mind::k_SleepByObjectHeights * situation.height,
				                 !situation.controlledByScript);
			}
			if (const auto clear = EmoteClearArea(creature, *executor, point, situation))
			{
				situation.clearArea = *clear;
			}
			SituateExploration(creature, *executor, situation);
			if (executor->build == creature_plan_actions::Build::DanceWithVillagers)
			{
				SituateDance(creature, *executor, point, situation);
			}
			mind_town_help::Situate(registry, creature, *executor,
			                        plan.object.has_value() ? std::optional(entt::to_integral(*plan.object)) : std::nullopt,
			                        situation);
			mind_football::Situate(registry, creature, *executor,
			                       plan.object.has_value() ? std::optional(entt::to_integral(*plan.object)) : std::nullopt,
			                       situation);
			if (plan.object.has_value())
			{
				SituateFriend(creature, *executor, *plan.object, situation);
			}
			DrawAgendaLengths(*executor, situation, _random.GameFloat());
			const auto actionIndex = creature_mind_tables::FindAction(*tables, plan.action);
			const auto cast = creature_plan_actions::IsCast(*executor) && actionIndex.has_value()
			                      ? CastInfoFor(creature, *actionIndex, plan.object)
			                      : std::nullopt;
			agenda = creature_plan_actions::Agenda(
			    *executor,
			    AgendaThingOf(registry, *executor,
			                  plan.object.has_value() ? std::optional(entt::to_integral(*plan.object)) : std::nullopt),
			    point, situation, random, cast);
			if (agenda.has_value())
			{
				BelieveInTempleToEnter(creature, *executor, situation);
			}
			else if (executor->build == creature_plan_actions::Build::Drink && mind->desires.has_value())
			{
				creature_drink::NoPlaceFound(*mind->desires, k_TurnsPerSecond);
			}
			activity = executor->activity;
		}
	}
	if (!agenda.has_value())
	{
		return false;
	}
	// The area that holds the creature checks each walk of the new agenda. An agenda that the action makes fails when the
	// area refuses one of its walks, and the creature does not want the action while the area holds it. An agenda that
	// the forced plan gives itself only loses the walks that the area refuses.
	const auto action = creature_mind_tables::FindAction(*tables, plan.action);
	bool refused = false;
	if (const auto area = AgendaAreaOf(registry, creature, plan.action, plan.walksBack); area.has_value())
	{
		if (plan.agenda.has_value())
		{
			agenda = creature_agenda_area::WithoutRefusedWalks(area->first, area->second, std::move(*agenda));
		}
		else
		{
			refused = creature_agenda_area::RefusesAgenda(area->first, area->second, *agenda);
		}
	}
	if (refused && action.has_value())
	{
		mind->leftAreaActions.resize(std::max(mind->leftAreaActions.size(), tables->actions.size()), false);
		mind->leftAreaActions[*action] = true;
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} cannot walk out of its area to {}", entt::to_integral(creature),
		                    plan.action);
	}
	mind->agendaRefused = refused;
	// The forced plan stops what the hands do, as each plan stop does. A point ends only when the plan also ends the
	// action of the body.
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
	{
		Locator::creatureObjectActionSystem::value().StopForPlan(creature, plan.endsBody);
	}
	if (!Replan(creature, activity, std::move(*agenda), plan.stopsWalk, plan.endsBody))
	{
		mind->agendaRefused = false;
		return false;
	}
	// Forced, nothing it wants is ever more pressing than the plan until it is done
	if (!action.has_value())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} carries out {}, which the action table hasn't",
		                    entt::to_integral(creature), plan.action);
		return true;
	}
	const auto object = plan.object.has_value() ? std::optional(entt::to_integral(*plan.object)) : std::nullopt;
	mind->planner.current = creature_planner::Plan {.desire = plan.desire,
	                                                .action = *action,
	                                                .object = object,
	                                                .priority = plan.priority.value_or(std::numeric_limits<float>::max())};
	mind->planActive = true;
	mind->planForced = plan.forced;
	mind->planSerial = mind->idle.serial;
	mind->agendaSeen = mind->idle.serial;
	mind->desireSeenTo = false;
	mind->planActions = tables->desireActions.at(static_cast<size_t>(plan.desire));
	// A plan that failed at once is not remembered for the player's feedback, and holds back no desire
	if (refused)
	{
		return true;
	}
	const auto& info = tables->actions[*action];
	// TODO(usefulness-use-tree): the game's forced plans each name what their desire is about; only the give passes it
	// here yet, and the rest count as the planner's own plans do
	const auto lessonActivity = plan.activityObject.has_value() ? std::optional(entt::to_integral(*plan.activityObject))
	                                                            : LessonActivity(*tables, plan.desire, object, std::nullopt);
	// It is remembered for the player's feedback, unless nothing says why it wants what the plan serves
	if (const auto source = creature_learning::PlanSource((*mind->desires)[plan.desire]); source.has_value())
	{
		RememberAction(creature, *mind,
		               {
		                   .action = *action,
		                   .desire = plan.desire,
		                   .object = object,
		                   .belief = plan.object.has_value() ? ActedOnBeliefOf(registry, *plan.object, creature) : std::nullopt,
		                   .activity = lessonActivity,
		                   .activityBelief = lessonActivity.has_value()
		                                         ? BeliefOf(registry, static_cast<entt::entity>(*lessonActivity), creature)
		                                         : std::nullopt,
		                   .learnable = info.learnable,
		                   .windowSeconds = info.learningWindowSeconds,
		                   .source = *source,
		                   .ordered = plan.ordered,
		               });
	}
	if (plan.suppressesOpposed)
	{
		creature_learning::SuppressOpposed(*mind->desires, plan.desire, tables->dependencies, k_TurnsPerSecond);
	}
	return true;
}

bool CreatureMindSystem::CarryOutForScript(entt::entity creature, std::vector<creature_mind::Step> agenda)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr)
	{
		return false;
	}
	// The script takes it from a fight, from lying knocked out and from what its hands do first
	StopForScript(creature);
	if (!Replan(creature, creature_mind::Activity::Told, std::move(agenda)))
	{
		return false;
	}
	registry.AssignOrReplace<ScriptControlled>(creature);
	// It obeys its player by following them, as far as its plan goes, which nothing it wants replaces until the agenda
	// is over; unlike an order, it learns nothing from it
	if (const auto* tables = GetTables(); tables != nullptr)
	{
		if (const auto action = creature_mind_tables::FindAction(*tables, "FollowPlayer"); action.has_value())
		{
			mind->planner.current = creature_planner::Plan {.desire = creature_desires::Desire::ObeyPlayer,
			                                                .action = *action,
			                                                .object = entt::to_integral(creature),
			                                                .priority = std::numeric_limits<float>::max()};
		}
	}
	mind->planActive = true;
	mind->planSerial = mind->idle.serial;
	mind->agendaSeen = mind->idle.serial;
	mind->desireSeenTo = false;
	return true;
}

void CreatureMindSystem::PlanDeath(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	const auto* tables = GetTables();
	if (mind == nullptr || tables == nullptr)
	{
		return;
	}
	const auto action = creature_mind_tables::FindAction(*tables, k_DeathAction);
	if (!action.has_value())
	{
		return;
	}
	// The knock-out does the steps of the plan, so the mind does not start an agenda for it. The plan stays until the
	// creature chooses or gets a new plan after the knock-out.
	mind->planner.current = creature_planner::Plan {.desire = creature_desires::Desire::ObeyPlayer,
	                                                .action = *action,
	                                                .object = entt::to_integral(creature),
	                                                .priority = std::numeric_limits<float>::max()};
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} lies dead by its plan", entt::to_integral(creature));
}

void CreatureMindSystem::SetScriptFocus(entt::entity creature, std::optional<creature_mind::Gaze> focus)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr; mind != nullptr)
	{
		mind->scriptFocus = focus;
	}
}
