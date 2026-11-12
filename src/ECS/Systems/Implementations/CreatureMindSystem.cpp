/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureMindSystem.h"

#include <cmath>
#include <cstring>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iterator>
#include <limits>
#include <random>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/DayNightClock.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureAnimation.h"
#include "Creature/CreatureAttention.h"
#include "Creature/CreatureDesireSources.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureDevelopment.h"
#include "Creature/CreatureEmergencies.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFriendship.h"
#include "Creature/CreatureGiving.h"
#include "Creature/CreatureHandRules.h"
#include "Creature/CreatureHelp.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureKindAgendas.h"
#include "Creature/CreatureKiss.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureLostThing.h"
#include "Creature/CreatureMeals.h"
#include "Creature/CreatureMode.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureOddAgendas.h"
#include "Creature/CreaturePhysiology.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreaturePlayerAttention.h"
#include "Creature/CreatureReach.h"
#include "Creature/CreatureRestAndHome.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/CreatureThrow.h"
#include "Creature/CreatureTownHelp.h"
#include "Creature/CreatureTurnOrder.h"
#include "CreatureMindSystemDetail.h"
#include "CreatureMindTownHelp.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureGestureDrawing.h"
#include "ECS/Components/CreatureHelp.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/CreatureCarried.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/CreatureHome.h"
#include "ECS/LearningMarker.h"
#include "ECS/Map.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/PlayerTowns.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/ReportedCamera.h"
#include "ECS/ShownCreatureFaces.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureAnimationLogSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/PlaygroundTripSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicWorldInterface.h"
#include "MagicLiving.h"
#include "ObjectMeasures.h"
#include "Physics/Body.h"
#include "Physics/LivingRules.h"
#include "VillagerFire.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace animations = openblack::creature_layers::animations;

namespace openblack::ecs::systems::mind_detail
{
void TellFainting(entt::entity creature)
{
	if (openblack::Locator::creatureHelpSystem::has_value())
	{
		openblack::Locator::creatureHelpSystem::value().Fainting(creature);
	}
}
} // namespace openblack::ecs::systems::mind_detail

using creature_desires::Desire;

namespace openblack::ecs::systems::mind_detail
{
constexpr float k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
constexpr float k_TurnsPerSecond = 1.0f / k_TurnSeconds;
/// The whole game turns in a second, as the game counts them
constexpr auto k_WholeTurnsPerSecond = static_cast<uint32_t>(1000 / TimeSystemInterface::k_TurnDuration.count());

/// A face told to by hand is held this long
constexpr float k_ToldFaceMs = 3000.0f;

/// A creature looks for food and water this far away at most
constexpr float k_FoodSearchDistance = 200.0f;
/// Food further than this from its mouth, for its size, has got away by the time it eats
constexpr float k_EatReach = 15.0f;

constexpr std::string_view k_PukeAction = "Puke";

constexpr std::string_view k_FightAction = "Fight";
/// The action of a creature that obeys another creature, as the other creature in a fight does
constexpr std::string_view k_ObeyCreatureAction = "ObeyCreature";
/// The action a creature made to catch something carries out
constexpr std::string_view k_CatchAction = "Catch";
/// The action a creature given something from the hand carries out to take it
constexpr std::string_view k_TakeFromHandAction = "TakeObjectFromHand";
/// What a creature does with what it was given, by the game's action table
constexpr std::string_view k_ExamineGivenAction = "ExamineObjectInHand";
constexpr std::string_view k_EatGivenAction = "EatObjectInHand";
constexpr std::string_view k_ThrowGivenAction = "ThrowObjectInHand";

void ReleaseCast(entt::entity creature)
{
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().ReleaseCreatureCast(creature);
	}
}

void NoteRefused(entt::entity creature, creature_animation_log::Layer layer, size_t clip, size_t playing)
{
	if (Locator::creatureAnimationLog::has_value())
	{
		Locator::creatureAnimationLog::value().Refused(creature, layer, clip, "mind", playing);
	}
}

creature_face::Feelings FeelingsOf(const CreatureMindState& mind)
{
	return {.variety = mind.idle.faceVariety, .attitudeToPlayer = mind.attitudeToPlayer, .idlePick = 0};
}

/// What something is worth to eat, if anything
std::optional<float> FoodValueOf(entt::entity entity)
{
	return Locator::creatureObjectActionSystem::has_value() ? Locator::creatureObjectActionSystem::value().FoodValueOf(entity)
	                                                        : std::nullopt;
}

/// The nearest food that no creature holds, in the search distance from a point, and its position
std::optional<std::pair<entt::entity, glm::vec2>> NearestFood(ecs::Registry& registry, glm::vec2 from)
{
	std::optional<std::pair<entt::entity, glm::vec2>> nearest;
	float best = k_FoodSearchDistance;
	const auto consider = [&](entt::entity entity, const Transform& at) {
		const glm::vec2 point {at.position.x, at.position.z};
		const auto distance = glm::distance(point, from);
		if (distance <= best && !registry.AllOf<HeldByCreature>(entity) && FoodValueOf(entity).has_value())
		{
			best = distance;
			nearest = {entity, point};
		}
	};
	registry.Each<const Villager, const Transform>(
	    [&](entt::entity entity, const Villager&, const Transform& at) { consider(entity, at); });
	registry.Each<const MobileObject, const Transform>(
	    [&](entt::entity entity, const MobileObject&, const Transform& at) { consider(entity, at); });
	registry.Each<const Pot, const Transform>(
	    [&](entt::entity entity, const Pot&, const Transform& at) { consider(entity, at); });
	return nearest;
}
} // namespace openblack::ecs::systems::mind_detail

using namespace openblack::ecs::systems::mind_detail;

void CreatureMindSystem::SaveState(reflection::BinaryWriter& out) const
{
	reflection::Save(out, _averageFeedback);
	reflection::Save(out, _orderedPlanFor);
}

bool CreatureMindSystem::LoadState(reflection::BinaryReader& in)
{
	reflection::Load(in, _averageFeedback);
	reflection::Load(in, _orderedPlanFor);
	return !in.Failed();
}

void CreatureMindSystem::ProcessTurn()
{
	BeginTurn();
	std::vector<entt::entity> creatures;
	Locator::entitiesRegistry::value().Each<const Creature, CreatureMindState, CreatureAnimation, const Transform>(
	    [&creatures](entt::entity entity, const Creature& /*creature*/, CreatureMindState& /*mind*/,
	                 CreatureAnimation& /*animation*/, const Transform& /*transform*/) { creatures.push_back(entity); });
	for (const auto creature : creatures)
	{
		MindTurn(creature);
		DesiresTurn(creature);
		AgendaTurn(creature);
	}
}

void CreatureMindSystem::BeginTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	// The marks over learning creatures whose life is over go at the turn, the same turn on every computer
	ecs::learning_marker::RemoveDone(registry, Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u);
	// The feedback of a release reaches each mind before the minds think, when its wait is over
	std::vector<std::pair<entt::entity, float>> arrived;
	registry.Each<CreatureMindState>([&arrived](entt::entity entity, CreatureMindState& mind) {
		if (!mind.releaseFeedback.has_value())
		{
			return;
		}
		if (mind.releaseFeedbackTurns > 0)
		{
			--mind.releaseFeedbackTurns;
			return;
		}
		arrived.emplace_back(entity, *std::exchange(mind.releaseFeedback, std::nullopt));
	});
	for (const auto& [entity, feedback] : arrived)
	{
		ReceiveFeedback(entity, feedback);
	}
}

bool CreatureMindSystem::PlayAction(entt::entity creature, size_t animation)
{
	auto* body = Locator::entitiesRegistry::value().TryGet<CreatureAnimation>(creature);
	if (body == nullptr)
	{
		return false;
	}
	const auto played = creature_layers::PlayOnce(body->body, animation, _random.GameRand(2) == 1);
	if (played.has_value())
	{
		body->body = *played;
	}
	else
	{
		NoteRefused(creature, creature_animation_log::Layer::Body, animation, creature_layers::CurrentAnimation(body->body));
	}
	return played.has_value();
}

bool CreatureMindSystem::PlayGesture(entt::entity creature, size_t animation)
{
	auto* body = Locator::entitiesRegistry::value().TryGet<CreatureAnimation>(creature);
	if (body == nullptr)
	{
		return false;
	}
	const auto played = creature_layers::PlayGesture(body->gesture, animation);
	if (played.has_value())
	{
		body->gesture = *played;
	}
	else
	{
		NoteRefused(creature, creature_animation_log::Layer::Gesture, animation,
		            body->gesture.animation.value_or(creature_layers::animations::k_Stand));
	}
	return played.has_value();
}

void CreatureMindSystem::PullFace(entt::entity creature, size_t animation)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* body = registry.TryGet<CreatureAnimation>(creature))
	{
		shown_creature_faces::ChangeFace(registry, creature, body->face, [animation](const creature_layers::FaceLayer& face) {
			return creature_layers::PullFace(face, animation, k_ToldFaceMs, creature_face::Cue::Told);
		});
	}
}

std::optional<creature_face::Request> CreatureMindSystem::ShowFeeling(entt::entity creature, creature_face::Cue cue)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* body = registry.TryGet<CreatureAnimation>(creature);
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (body == nullptr || mind == nullptr)
	{
		return std::nullopt;
	}
	const auto random = [this](uint32_t range) { return _random.GameRand(range); };
	// The agendas, the faces and the sides draw on the same synced seed of the game
	const auto& gameRandom = random;
	const creature_mind::Senses senses {.feelings = FeelingsOf(*mind)};
	const auto face = creature_mind::PullFace(mind->idle, cue, senses, random, gameRandom);
	if (face.has_value())
	{
		shown_creature_faces::ChangeFace(registry, creature, body->face, [&face, cue](const creature_layers::FaceLayer& layer) {
			return creature_layers::PullFace(layer, creature_face::AnimationOf(face->face), face->milliseconds, cue);
		});
	}
	return face;
}

bool CreatureMindSystem::SitDown(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* body = registry.TryGet<CreatureAnimation>(creature);
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (body == nullptr || mind == nullptr || creature_layers::IsPlaying(body->body))
	{
		return false;
	}
	// A paused mind won't get it up again: it sits until told to stand
	if (mind->paused)
	{
		body->body =
		    *creature_layers::PlaySequence(body->body, animations::k_StartSit, animations::k_Sit, animations::k_EndSit);
		return true;
	}
	Abandon(creature, *mind);
	creature_mind::Plan(mind->idle, creature_mind::Activity::Told, {creature_mind::SitDown(_random.GameFloat())});
	return true;
}

void CreatureMindSystem::UpdateAttitudeFromFeedback(entt::entity creature, float feedback)
{
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return;
	}
	// Good feedback shows it its player wants compassion, bad anger, and it warms or cools to the player
	creature_perceived_desires::Increase(
	    mind->perceivedDesires, static_cast<size_t>(feedback > 0.0f ? Desire::Compassion : Desire::Anger), std::abs(feedback));
	// The scripts read how strongly it was last rewarded or punished
	mind->interactionMagnitude = feedback;
	mind->attitudeToPlayer = creature_feedback::AttitudeAfter(mind->attitudeToPlayer, feedback);
	// The average of the feedback is one for all creatures
	_averageFeedback = creature_feedback::AverageAfter(_averageFeedback, feedback);
}

void CreatureMindSystem::TurnBeforeDecision(entt::entity creature, const Creature& body, CreatureMindState& mind, bool scripted)
{
	// Before the first stage, the desires of its body stay at 0
	if (mind.desires.has_value())
	{
		creature_turn_order::HoldBodyDesires(*mind.desires, BodyDesires(), mind.developmentPhase);
	}
	const auto planHasSteps = [&mind]() {
		return mind.planActive && mind.planner.current.has_value() && mind.idle.serial == mind.planSerial &&
		       mind.idle.step < mind.idle.agenda.size();
	};
	// After a hold, the first plan that has steps to do starts again from its first step
	if (creature_turn_order::RestartsAfterHold(mind.heldSinceRestart, planHasSteps()))
	{
		mind.heldSinceRestart = false;
		// The plan stops first: a fight stops with no response, and the fight of the opponent stops too
		if (Locator::creatureFightSystem::has_value())
		{
			Locator::creatureFightSystem::value().HandLetGo(creature);
		}
		StartPlanAgain(creature, mind);
	}
	if (planHasSteps() && creature_turn_order::OwnPlanRuns(true, mind.planner.current->desire))
	{
		mind.ownPlanSinceStop = true;
	}
	// The copy timeout applies only to the creature of a player that is not the computer, and not under a script
	const bool computer = Locator::playerSystem::has_value() && Locator::playerSystem::value().IsComputerPlayer(body.owner);
	if (body.owner == PlayerNames::NEUTRAL || computer || scripted)
	{
		return;
	}
	if (creature_turn_order::StepCopyTimeout(mind.turnsWithoutOwnPlan, mind.ownPlanSinceStop, k_WholeTurnsPerSecond))
	{
		// It stops copying the player, the plan is no longer forced, and it does not follow the player for a time
		if (mind.learnt.has_value())
		{
			mind.learnt->mimicry.reset();
		}
		mind.planForced = false;
		if (mind.desires.has_value())
		{
			creature_desires::Suppress(*mind.desires, Desire::FollowPlayerDesire,
			                           creature_turn_order::k_NoFollowAfterCopyTimeoutSeconds, k_TurnsPerSecond);
		}
	}
}

void CreatureMindSystem::StartPlanAgain(entt::entity creature, CreatureMindState& mind)
{
	const auto plan = *mind.planner.current;
	// The context of the plan stays as it was for the player's feedback
	const bool contextRunning =
	    mind.learnt.has_value() && !mind.learnt->contexts.empty() && mind.learnt->contexts.back().running;
	// An action that the body started plays on, and a point plays its end
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
	{
		Locator::creatureObjectActionSystem::value().StopForPlan(creature, true);
	}
	creature_plan_actions::Situation situation {.instrument = plan.instrument, .activity = plan.activityObject};
	Situate(creature, situation);
	auto built = PlanAgenda(creature, plan.action, plan.object, situation);
	if (!built.has_value())
	{
		// With no agenda, the plan stops and the creature decides again
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} has no agenda to start {} again after the hand let go",
		                    entt::to_integral(creature), plan.action);
		Abandon(creature, mind);
		return;
	}
	auto& [executor, agenda] = *built;
	if (!Replan(creature, executor->activity, std::move(agenda)))
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} cannot start {} again after the hand let go",
		                    entt::to_integral(creature), plan.action);
		return;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} starts {} again after the hand let go", entt::to_integral(creature),
	                   plan.action);
	mind.planner.current = plan;
	mind.planActive = true;
	mind.planSerial = mind.idle.serial;
	mind.agendaSeen = mind.idle.serial;
	mind.desireSeenTo = false;
	if (contextRunning)
	{
		mind.learnt->contexts.back().running = true;
	}
}

void CreatureMindSystem::NoteLeashOrder(entt::entity creature)
{
	if (auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature))
	{
		mind->attention.turnsSinceInteraction = 0;
	}
}

void CreatureMindSystem::HandTookHold(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return;
	}
	// The hold stops a dance, a pose that loops and a point. A fight ends on the first turn of the hold.
	if (auto* body = registry.TryGet<CreatureAnimation>(creature); body != nullptr)
	{
		body->body = creature_layers::EndDance(creature_layers::EndLoop(body->body));
	}
	if (Locator::creatureObjectActionSystem::has_value())
	{
		Locator::creatureObjectActionSystem::value().EndPoint(creature);
	}
	// It stops and turns to face the player's eye. While its body plays an action, the turn waits until the action ends.
	if (const auto camera = openblack::ecs::ReportedCameraOfItsPlayer(creature);
	    Locator::creatureLocomotionSystem::has_value() && camera.has_value())
	{
		Locator::creatureLocomotionSystem::value().TurnToFaceAtRest(creature, glm::vec2(camera->origin.x, camera->origin.z));
	}
	mind->showDesireOnHold = true;
}

void CreatureMindSystem::ShowDesireOnHold(entt::entity creature, CreatureMindState& mind, CreatureAnimation& animation)
{
	const auto* locomotion =
	    Locator::creatureLocomotionSystem::has_value() ? &Locator::creatureLocomotionSystem::value() : nullptr;
	if (!creature_turn_order::ShowsDesireOnHold(creature_layers::IsPlaying(animation.body),
	                                            locomotion != nullptr && locomotion->IsMoving(creature),
	                                            locomotion != nullptr && locomotion->TurnWaitsForRest(creature)))
	{
		return;
	}
	mind.showDesireOnHold = false;
	std::optional<Desire> credited;
	if (mind.learnt.has_value())
	{
		if (const auto best = creature_learning::BestContext(mind.learnt->contexts); best.has_value())
		{
			credited = mind.learnt->contexts.at(*best).desire;
		}
	}
	const auto planned =
	    mind.planActive && mind.planner.current.has_value() ? std::optional(mind.planner.current->desire) : std::nullopt;
	const auto desire = creature_help::DesireShownOnHold(credited, planned);
	if (!desire.has_value())
	{
		return;
	}
	PlayAction(creature, *creature_desires::EmoteFor(*desire));
	// The help says what it wants, even though the hand is on it
	if (Locator::creatureHelpSystem::has_value())
	{
		Locator::creatureHelpSystem::value().Raise(
		    creature,
		    {.kind = creature_help::Kind::CurrentDesire, .parameter = static_cast<uint32_t>(*desire), .forced = true});
	}
}

void CreatureMindSystem::SendReleaseFeedback(entt::entity creature, float feedback)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return;
	}
	// The feedback of an earlier release arrives first
	if (const auto earlier = std::exchange(mind->releaseFeedback, std::nullopt); earlier.has_value())
	{
		ReceiveFeedback(creature, *earlier);
		mind = registry.TryGet<CreatureMindState>(creature);
		if (mind == nullptr)
		{
			return;
		}
	}
	mind->releaseFeedback = feedback;
	mind->releaseFeedbackTurns = creature_feedback::k_ReleaseFeedbackTurns;
}

void CreatureMindSystem::ReceiveFeedback(entt::entity creature, float feedback)
{
	namespace sources = creature_desires::sources;
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return;
	}
	mind->lastFeedback = CreatureMindState::Feedback {.value = feedback, .activity = mind->idle.activity};
	// Let go of, however slightly it was stroked or slapped, it stops whatever it was doing: it stops walking, puts down
	// what it was doing with a thing, drops its agenda and gets up out of a sitting or lying pose. An action it is playing,
	// such as reeling from the last slap, plays on. An action that the body started plays on, and a point plays its end.
	// A fight that its plan started again after the hold stops for both creatures, with no winner.
	if (registry.AllOf<CreatureFighting>(creature) && Locator::creatureFightSystem::has_value())
	{
		Locator::creatureFightSystem::value().CallOffForPlan(creature);
	}
	if (Locator::creatureObjectActionSystem::has_value())
	{
		Locator::creatureObjectActionSystem::value().StopForPlan(creature, true);
	}
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().StopAtOnce(creature);
	}
	mind->idle.agenda.clear();
	mind->idle.step = 0;
	mind->idle.stepStarted = false;
	if (auto* body = registry.TryGet<CreatureAnimation>(creature);
	    body != nullptr && body->body.kind == creature_layers::BodyAction::Kind::Sequence)
	{
		body->body = creature_layers::EndLoop(body->body);
	}
	if (auto* body = registry.TryGet<CreatureAnimation>(creature); body != nullptr)
	{
		body->body = creature_layers::EndDance(body->body);
	}
	// The stop also lets go of a miracle that the creature holds, and its eyes go calm
	ReleaseCast(creature);
	RestEyes(registry, creature, registry.TryGet<CreatureEyes>(creature));
	// The stop also ends a faint: a knocked-out creature gets up where it lies
	if (Locator::creatureFightSystem::has_value())
	{
		Locator::creatureFightSystem::value().GetUpForPlan(creature);
	}
	// Too slight to count, it only looks at the camera of its player, for its desire to idle with the player
	if (std::abs(feedback) <= creature_feedback::k_SlightFeedback)
	{
		if (openblack::ecs::ReportedCameraOfItsPlayer(creature).has_value())
		{
			constexpr auto k_Look = creature_feedback::k_SlightFeedbackLook;
			static_cast<void>(ForcePlan(creature, {.desire = creature_desires::Desire::IdleWithPlayer,
			                                       .action = k_Look.action,
			                                       .object = creature,
			                                       .ordered = k_Look.ordered,
			                                       .forced = k_Look.forced,
			                                       .priority = k_Look.priority,
			                                       .fightBodyEndsAgain = true}));
		}
		return;
	}
	UpdateAttitudeFromFeedback(creature, feedback);
	mind->feedbackSeconds = 0.0f;
	mind->feedbackWasStroke = feedback > 0.0f;
	LearnFromFeedback(creature, *mind, feedback);
}

/// How far a body is through what it plays: the time of its action, or of its legs in a move, over the length of the
/// animation that the game keeps for it
static std::optional<float> BodyShareOf(entt::entity creature, const CreatureAnimation& body)
{
	// A creature in a move counts as a body that does an action: the time of its legs is read
	const auto* legs = Locator::entitiesRegistry::value().TryGet<const CreatureLocomotion>(creature);
	const bool moves = legs != nullptr && legs->motion != CreatureLocomotion::Motion::Standing;
	const auto share = creature_layers::ShareOf(body.body, moves && legs->motion == CreatureLocomotion::Motion::Confused,
	                                            moves ? std::optional(legs->legsTimeMs) : std::nullopt);
	if (!share.has_value() || !Locator::creatureAnimationSystem::has_value())
	{
		return std::nullopt;
	}
	// The time and the duration are whole milliseconds, so the float divide gives the same share as the game
	const auto duration = Locator::creatureAnimationSystem::value().AnimationDuration(creature, share->animation);
	if (!duration.has_value() || !(*duration > 0.0f))
	{
		return std::nullopt;
	}
	return share->timeMs / *duration;
}

std::optional<float> CreatureMindSystem::BodyShare(entt::entity creature) const
{
	const auto* body = Locator::entitiesRegistry::value().TryGet<const CreatureAnimation>(creature);
	if (body == nullptr)
	{
		return std::nullopt;
	}
	if (const auto share = BodyShareOf(creature, *body); share.has_value())
	{
		return share;
	}
	// A body that only stands shows its stand at the point of its breath. The script reads that share, so it goes up
	// and down with each breath after an action ends.
	if (!Locator::creatureAnimationSystem::has_value())
	{
		return std::nullopt;
	}
	const auto stand =
	    Locator::creatureAnimationSystem::value().AnimationDuration(creature, creature_layers::animations::k_Stand);
	if (!stand.has_value() || !(*stand > 0.0f))
	{
		return std::nullopt;
	}
	return creature_animation::StandShare(body->breathPhase, static_cast<uint32_t>(*stand));
}

namespace
{
/// Whether a knock-out keeps the creature out cold. A stroke or a slap that played then would cut the faint before the
/// body lies still, and the faint would never end.
bool OutCold(const ecs::Registry& registry, entt::entity creature)
{
	const auto* knockedOut = registry.TryGet<const CreatureKnockedOut>(creature);
	return knockedOut != nullptr && creature_hand::OutColdForHand(knockedOut->stage);
}
} // namespace

bool CreatureMindSystem::MayInterrupt(entt::entity creature, const CreatureAnimation& body, double interruptsAfter)
{
	const auto objectActionShare = Locator::creatureObjectActionSystem::has_value()
	                                   ? Locator::creatureObjectActionSystem::value().GetProgress(creature)
	                                   : std::nullopt;
	return creature_feedback::TakesForcedAction(objectActionShare, BodyShareOf(creature, body), interruptsAfter);
}

bool CreatureMindSystem::CanForceAction(entt::entity creature, size_t animation, bool mirrored, double interruptsAfter) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const CreatureAnimation>(creature);
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	if (body == nullptr || mind == nullptr || creature_mind::IsUnconscious(mind->idle) || OutCold(registry, creature))
	{
		return false;
	}
	return MayInterrupt(creature, *body, interruptsAfter) &&
	       creature_layers::PlayOnce(creature_layers::BodyAction {}, animation, mirrored).has_value();
}

bool CreatureMindSystem::ForceAction(entt::entity creature, size_t animation, bool mirrored, double interruptsAfter)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* body = registry.TryGet<CreatureAnimation>(creature);
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (body == nullptr || mind == nullptr || creature_mind::IsUnconscious(mind->idle) || OutCold(registry, creature))
	{
		return false;
	}
	if (!MayInterrupt(creature, *body, interruptsAfter))
	{
		return false;
	}
	const auto played = creature_layers::PlayOnce(creature_layers::BodyAction {}, animation, mirrored);
	if (!played.has_value())
	{
		return false;
	}
	if (Locator::creatureObjectActionSystem::has_value())
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().StopAtOnce(creature);
	}
	body->body = *played;
	// Only the body is taken: the mind goes on with what it was doing once the action has played, as its agenda waits for
	// a busy body
	return true;
}

bool CreatureMindSystem::Sleep(entt::entity creature)
{
	return Replan(creature, creature_mind::Activity::Sleep,
	              creature_mind::Sleep([this](uint32_t range) { return _random.GameRand(range); }));
}

bool CreatureMindSystem::Eat(entt::entity creature, std::optional<entt::entity> food)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (!food.has_value() && transform != nullptr)
	{
		if (const auto nearest = NearestFood(registry, glm::vec2(transform->position.x, transform->position.z)))
		{
			food = nearest->first;
		}
	}
	if (!food.has_value() || !FoodValueOf(*food).has_value())
	{
		return false;
	}
	const bool holdingEatable =
	    Locator::creatureObjectActionSystem::has_value() && Locator::creatureObjectActionSystem::value().CanEatHeld(creature);
	return Replan(creature, creature_mind::Activity::Eat, creature_mind::Eat(entt::to_integral(*food), holdingEatable));
}

bool CreatureMindSystem::Drink(entt::entity creature)
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(creature);
	if (transform == nullptr)
	{
		return false;
	}
	const auto* body = Locator::entitiesRegistry::value().TryGet<const Creature>(creature);
	const auto place = DrinkPlaceOf(creature);
	return body != nullptr && place.has_value() &&
	       Replan(creature, creature_mind::Activity::Drink,
	              creature_drink::Drink(*place, creature_mode::CreatureHeight(ShownSize(*body))));
}

bool CreatureMindSystem::Poo(entt::entity creature)
{
	return Replan(creature, creature_mind::Activity::Poo,
	              creature_mind::Poo([this](uint32_t range) { return _random.GameRand(range); }, std::nullopt));
}

bool CreatureMindSystem::Puke(entt::entity creature)
{
	// The creature is sick on the spot. The plan is an order and a forced plan, so the creature accepts no reaction
	// until the plan ends.
	return ForcePlan(creature, {.desire = Desire::Puke,
	                            .action = k_PukeAction,
	                            .object = creature,
	                            .agenda = creature_mind::Puke(),
	                            .activity = creature_mind::Activity::Puke,
	                            .ordered = true,
	                            .fightBodyEndsAgain = true});
}

bool CreatureMindSystem::Faint(entt::entity creature)
{
	// It faints as a fight's loser does: it lies where it fell, then a player's creature is carried to its pen to come
	// round
	auto& registry = Locator::entitiesRegistry::value();
	if (Locator::creatureFightSystem::has_value() && registry.Valid(creature))
	{
		TellFainting(creature);
		Locator::creatureFightSystem::value().ForceFaint(creature);
		return registry.AllOf<CreatureKnockedOut>(creature);
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	return Replan(creature, creature_mind::Activity::Faint, creature_mind::Faint(body != nullptr ? ShownSize(*body) : 1.0f));
}

void CreatureMindSystem::Heal(entt::entity creature, entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing))
	{
		return;
	}
	// The thing takes a full heal from the creature, which moves the creature's alignment as a heal does
	if (Locator::magicSystem::has_value())
	{
		magic::EffectValues values;
		values[magic::EffectKind::Heal] = 1.0f;
		const auto* body = registry.TryGet<const Creature>(creature);
		Locator::magicSystem::value().ApplyEffectToObject(
		    thing, values,
		    magic::EffectSource {.player = body != nullptr ? body->owner : PlayerNames::NEUTRAL,
		                         .casterCreature = creature,
		                         .appliedBy = creature});
	}
	if (!registry.Valid(thing) || !magic_living::LifeOf(thing).has_value())
	{
		return;
	}
	// A living thing gets all its life back. A villager then goes home, and an animal wanders off.
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} heals {} from life {}", entt::to_integral(creature),
	                    entt::to_integral(thing), magic_living::LifeOf(thing).value_or(0.0f));
	magic_living::SetLife(thing, 1.0f);
	if (auto* action = registry.TryGet<LivingAction>(thing);
	    action != nullptr && registry.AllOf<Villager>(thing) && Locator::livingActionSystem::has_value())
	{
		static_cast<void>(Locator::livingActionSystem::value().VillagerChangeTopState(*action, VillagerStates::GoHome));
	}
	else if (registry.AllOf<Animal>(thing) && Locator::animalSystem::has_value())
	{
		Locator::animalSystem::value().StartWandering(thing);
	}
}

void CreatureMindSystem::Wake(entt::entity creature)
{
	if (auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature))
	{
		mind->idle.wakeWanted = true;
	}
}

void CreatureMindSystem::StandUp(entt::entity creature)
{
	if (auto* body = Locator::entitiesRegistry::value().TryGet<CreatureAnimation>(creature))
	{
		body->body = creature_layers::EndLoop(body->body);
	}
}

void CreatureMindSystem::FoughtFight(entt::entity creature, [[maybe_unused]] bool won)
{
	// Winning or losing, the fight is done. The game also has the loser learn from losing, which isn't known.
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	if (mind != nullptr && mind->desires.has_value())
	{
		Satisfied(creature, *mind->desires, k_FightAction);
	}
}

namespace
{
/// The agenda of a creature in a fight: one step that waits for the fight to end
std::vector<creature_mind::Step> FightAgenda(entt::entity other)
{
	return {creature_mind::Step {.kind = creature_mind::Step::Kind::Fight, .object = entt::to_integral(other)}};
}
} // namespace

void CreatureMindSystem::FightWith(entt::entity creature, entt::entity other, const ForcedPlan& plan)
{
	// The agenda of the creature waits for the fight to end. The fight has started already, so the step that starts it
	// is marked as started.
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<ScriptControlled>(creature) || !ForcePlan(creature, plan))
	{
		return;
	}
	if (auto* mind = registry.TryGet<CreatureMindState>(creature); mind != nullptr && !mind->idle.agenda.empty())
	{
		mind->idle.stepStarted = mind->idle.agenda.front().kind == creature_mind::Step::Kind::Fight &&
		                         mind->idle.agenda.front().object == std::optional(entt::to_integral(other));
	}
}

void CreatureMindSystem::FightStarted(entt::entity creature, entt::entity opponent, bool planned)
{
	// A creature told to fight, by the leash, a script or the debug tools, fights until the fight ends: nothing it wants
	// stops the fight.
	// TODO(creature-fight-mind): the leash makes the plan from the desires of the creature for the other creature. Until
	// the leash does that for creatures, the plan is to obey the player.
	if (!planned)
	{
		// A fight forced on a creature, like any forced action, makes every free creature agree to joint actions from
		// now on
		Locator::entitiesRegistry::value().Context().partnersAlwaysAgree = true;
		FightWith(creature, opponent,
		          {.desire = Desire::ObeyPlayer,
		           .action = k_FightAction,
		           .object = opponent,
		           .agenda = FightAgenda(opponent),
		           .ownFight = true});
	}
}

void CreatureMindSystem::FightOrdered(entt::entity creature, entt::entity asker)
{
	// The other creature stops what it does and obeys the first creature until the fight ends. Its plan is an order
	// that is not forced, with a low priority: it does not decide while it obeys. Each order gives the plan again, and
	// the creature remembers each one for the feedback of the player. The order holds back no desire.
	FightWith(creature, asker,
	          {.desire = Desire::ObeyCreature,
	           .action = k_ObeyCreatureAction,
	           .object = asker,
	           .agenda = FightAgenda(asker),
	           .ordered = true,
	           .forced = false,
	           .priority = creature_fight::k_ObeyPlanPriority * creature_planner::k_PriorityScale,
	           .ownFight = true,
	           .suppressesOpposed = false});
}

void CreatureMindSystem::ObeyEnded(entt::entity creature, entt::entity asker)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	if (mind == nullptr || !mind->planActive || !mind->planner.current.has_value() ||
	    mind->planner.current->desire != Desire::ObeyCreature)
	{
		return;
	}
	// A plan to obey another creature is a plan to obey the first creature of this fight. A first creature that left the
	// world is not known any more.
	if (asker != entt::null && mind->planner.current->object != std::optional(entt::to_integral(asker)))
	{
		return;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} stops to obey creature {}", entt::to_integral(creature),
	                    entt::to_integral(asker));
	AbandonAction(creature);
}

void CreatureMindSystem::EndFightForPlan(entt::entity creature, bool fightBodyEndsAgain)
{
	if (!Locator::creatureFightSystem::has_value())
	{
		return;
	}
	const auto& fights = Locator::creatureFightSystem::value();
	if (fights.IsFighting(creature) && !fights.IsDuelling(creature))
	{
		StopFightForPlan(creature, fightBodyEndsAgain);
	}
}

void CreatureMindSystem::StopFightForPlan(entt::entity creature, bool fightBodyEndsAgain)
{
	auto& fights = Locator::creatureFightSystem::value();
	if (fightBodyEndsAgain)
	{
		fights.CallOffForPlan(creature);
	}
	else
	{
		fights.CallOff(creature);
	}
}

void CreatureMindSystem::ForceCatch(entt::entity creature, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	const auto* tables = GetTables();
	if (mind == nullptr || tables == nullptr)
	{
		return;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} is made to catch {}", entt::to_integral(creature),
	                    entt::to_integral(object));
	// It comes across the thing that it catches. A forced plan first stops the reaction that the creature has, then
	// what it was doing stops as a failure.
	ComeAcross(creature, *mind, object);
	EndReaction(creature);
	Abandon(creature, *mind);
	ReleaseCast(creature);
	// When what it does fails, its body also stops its dance, its sitting or lying pose and its point (see Replan).
	// Each of these plays its end, which the catch waits for. An action that its body plays continues to its end.
	creature_mind::Step step {.kind = creature_mind::Step::Kind::Object};
	step.order = {.kind = creature_mind::ObjectOrder::Kind::Catch, .object = entt::to_integral(object)};
	if (!Replan(creature, creature_mind::Activity::Planned, {step}))
	{
		return;
	}
	const auto action = creature_mind_tables::FindAction(*tables, k_CatchAction);
	if (!action.has_value())
	{
		return;
	}
	mind->planner.current = creature_planner::Plan {.desire = Desire::Play,
	                                                .action = *action,
	                                                .object = entt::to_integral(object),
	                                                .goalUsefulness = creature_planner::k_CatchOrGiftScore,
	                                                .actionPriority = creature_planner::k_CatchOrGiftScore,
	                                                .priority = creature_planner::k_CatchOrGiftScore};
	mind->planActive = true;
	mind->planSerial = mind->idle.serial;
	mind->agendaSeen = mind->idle.serial;
	mind->desireSeenTo = false;
	if (!mind->desires.has_value())
	{
		return;
	}
	// The catch is remembered for the player's feedback, unless nothing says why the creature wants to play
	if (const auto source = creature_learning::PlanSource((*mind->desires)[Desire::Play]); source.has_value())
	{
		const auto& info = tables->actions[*action];
		RememberAction(creature, *mind,
		               {
		                   .action = *action,
		                   .desire = Desire::Play,
		                   .object = entt::to_integral(object),
		                   .belief = ActedOnBeliefOf(registry, object, creature),
		                   .learnable = info.learnable,
		                   .windowSeconds = info.learningWindowSeconds,
		                   .source = *source,
		               });
	}
	// Having decided to play, the desires that go against it are held back
	creature_learning::SuppressOpposed(*mind->desires, Desire::Play, tables->dependencies, k_TurnsPerSecond);
}

bool CreatureMindSystem::TakeFromHand(entt::entity creature, entt::entity object, bool leftHand, PlayerNames giver)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	const auto* tables = GetTables();
	if (mind == nullptr || tables == nullptr || !mind->desires.has_value() || !registry.Valid(object))
	{
		return false;
	}
	const auto action = creature_mind_tables::FindAction(*tables, k_TakeFromHandAction);
	if (!action.has_value())
	{
		return false;
	}
	// What it was doing stops as a failure. An action that the body started plays on, and a point plays its end.
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
	{
		Locator::creatureObjectActionSystem::value().StopForPlan(creature, true);
	}
	// The action that its body plays continues at two times the usual speed
	if (Locator::creatureAnimationSystem::has_value())
	{
		Locator::creatureAnimationSystem::value().Hurry(creature);
	}
	// Without a reaction, it keeps the plan that it does now. After it takes the thing, it can continue that
	// plan. With a reaction, it keeps the plan from the start of the reaction.
	if (const auto* reaction = registry.TryGet<const LivingReaction>(creature); reaction == nullptr || reaction->reaction == 0)
	{
		mind->keptPlan = mind->planner.current;
	}
	// The creature keeps the giver until the next giving. The turn before the take reads the giver's camera.
	mind->giver = giver;
	// The take stops a fight out of its duel. After a fight that is over, it ends the fight of the body again.
	EndFightForPlan(creature, true);
	Abandon(creature, *mind);
	ReleaseCast(creature);
	if (!Replan(creature, creature_mind::Activity::Planned, creature_mind::TakeFromHand(entt::to_integral(object), leftHand)))
	{
		return false;
	}
	// Curious about what it is given, the plan is on the creature itself
	const auto self = entt::to_integral(creature);
	mind->planner.current = creature_planner::Plan {.desire = Desire::Curiosity,
	                                                .action = *action,
	                                                .object = self,
	                                                .goalUsefulness = creature_planner::k_CatchOrGiftScore,
	                                                .actionPriority = creature_planner::k_CatchOrGiftScore,
	                                                .priority = creature_planner::k_CatchOrGiftScore};
	// To take the thing from the hand is a forced plan
	mind->planActive = true;
	mind->planForced = true;
	mind->planSerial = mind->idle.serial;
	mind->agendaSeen = mind->idle.serial;
	mind->desireSeenTo = false;
	creature_learning::SuppressOpposed(*mind->desires, Desire::Curiosity, tables->dependencies, k_TurnsPerSecond);
	return true;
}

bool CreatureMindSystem::IsActing(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	return mind != nullptr && (mind->planActive || !creature_mind::AgendaOver(mind->idle));
}

bool CreatureMindSystem::IsTakingFromHand(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	if (mind == nullptr || mind->idle.step >= mind->idle.agenda.size())
	{
		return false;
	}
	// The take is still ahead of it, or under way
	return std::ranges::any_of(mind->idle.agenda.begin() + static_cast<std::ptrdiff_t>(mind->idle.step),
	                           mind->idle.agenda.end(), [](const creature_mind::Step& step) {
		                           return step.kind == creature_mind::Step::Kind::Object &&
		                                  step.order.kind == creature_mind::ObjectOrder::Kind::TakeFromHand;
	                           });
}

void CreatureMindSystem::UseGift(entt::entity creature, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr || !mind->desires.has_value() || !registry.Valid(object) ||
	    !Locator::creatureObjectActionSystem::has_value() ||
	    Locator::creatureObjectActionSystem::value().GetHeld(creature) != object)
	{
		return;
	}
	// Grown up enough, it casts a one-shot globe that it was given
	if (registry.AllOf<OneOffSpellSeed>(object) && CastGivenGlobe(creature, *mind, object))
	{
		return;
	}
	// It comes across what it was given, then weighs it as a thing to act on for each desire it could serve; held, it
	// counts as near as can be
	ComeAcross(creature, *mind, object);
	if (ContinueKeptPlanWith(creature, *mind, object))
	{
		return;
	}
	const auto& desires = *mind->desires;
	const auto weigh = [&](Desire desire) {
		return creature_giving::Weighed {.desire = desires[desire].value,
		                                 .usefulness = ObjectToActOnUsefulness(creature, *mind, desire, object)};
	};
	// It remembers the sorts of object that it was given or played with, and that it learnt it cannot eat, by the object
	// type of their info
	const auto sort = mind_detail::ObjectSortOf(registry, object);
	const auto choice = creature_giving::ChooseUse({
	    .curiosity = weigh(Desire::Curiosity),
	    .hunger = weigh(Desire::Hunger),
	    .playWithPlayer = weigh(Desire::PlayWithPlayer),
	    .cantEatKind = sort.has_value() && mind->refusedFoodSorts.contains(*sort),
	    .givenKindBefore = sort.has_value() && mind->heldSortCounts.contains(*sort),
	});
	if (choice.remembersKind && sort.has_value())
	{
		mind->heldSortCounts[*sort] = 1;
	}
	const auto number = entt::to_integral(object);
	// The plan is about what the thing belongs to, if anything
	const auto owner = BelongsTo(object);
	switch (choice.use)
	{
	case creature_giving::Use::Examine:
		ForcePlan(creature, {.desire = Desire::Curiosity,
		                     .action = k_ExamineGivenAction,
		                     .object = object,
		                     .agenda = creature_mind::ExamineGiven(number),
		                     .activityObject = owner,
		                     .ordered = true,
		                     .fightBodyEndsAgain = true});
		break;
	case creature_giving::Use::Eat:
		ForcePlan(creature, {.desire = Desire::Hunger,
		                     .action = k_EatGivenAction,
		                     .object = object,
		                     .agenda = creature_mind::EatGiven(),
		                     .activityObject = owner,
		                     .ordered = true,
		                     .fightBodyEndsAgain = true});
		break;
	case creature_giving::Use::Throw:
	{
		const auto camera = openblack::ecs::ReportedCameraOfItsPlayer(creature);
		const auto eye = camera.has_value() ? camera->origin : glm::vec3(0.0f);
		ForcePlan(creature, {.desire = Desire::PlayWithPlayer,
		                     .action = k_ThrowGivenAction,
		                     .object = object,
		                     .agenda = creature_mind::ThrowGiven(eye),
		                     .activityObject = owner,
		                     .ordered = true,
		                     .fightBodyEndsAgain = true});
		break;
	}
	case creature_giving::Use::AsLed:
		ForcePlanOn(creature, object, true);
		break;
	}
}

void CreatureMindSystem::MoveToStage(entt::entity creature, uint32_t stage)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr)
	{
		return;
	}
	// A creature that its home keeps it near, with no leash on, is let go when it reaches the stage it may leave home
	// from
	if (Locator::leashSystem::has_value())
	{
		auto& leashes = Locator::leashSystem::value();
		const auto* leash = registry.TryGet<const CreatureLeash>(creature);
		if (leash != nullptr &&
		    creature_development::ReleasedFromHome(stage, leash->confinementRadius, leashes.IsLeashed(creature)))
		{
			leashes.ClearConfinement(creature);
		}
	}
	mind->developmentPhase = stage;
	// The time at the stage starts again
	if (mind->learnt.has_value())
	{
		mind->learnt->developmentTimer = 0;
	}
	// Its desires are set for the stage at once, even for the stage it is at already; a mind yet to think sets them up
	// for the stage when it first does
	if (mind->desires.has_value())
	{
		creature_desires::ActivateForPhase(*mind->desires, Phases(), stage);
		mind->desiresPhase = stage;
	}
	else
	{
		mind->desiresPhase.reset();
	}
	// The help clears a value that it keeps about the creature. A creature without help memory has the value 0 already.
	if (auto* help = registry.TryGet<CreatureHelpMemory>(creature))
	{
		creature_help::MovedToStage(help->memory);
	}
	Abandon(creature, *mind);
}
