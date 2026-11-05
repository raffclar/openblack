/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How a creature carries out its plan: each turn the agenda of the plan gives orders to the body, and the mind takes
// in what the body did

#define LOCATOR_IMPLEMENTATIONS

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
#include "Creature/CreatureDrink.h"
#include "Creature/CreatureEmergencies.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFriendship.h"
#include "Creature/CreatureGiving.h"
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
#include "CreatureMindSystem.h"
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

namespace
{
/// The action of the hands that an order of the mind starts, for the orders that walk to a thing
openblack::creature_object_actions::Kind HandsKindOf(openblack::creature_mind::ObjectOrder::Kind order)
{
	using Order = openblack::creature_mind::ObjectOrder::Kind;
	using openblack::creature_object_actions::Kind;
	switch (order)
	{
	case Order::PickUp:
		return Kind::PickUp;
	case Order::Destroy:
		return Kind::Destroy;
	case Order::Kick:
		return Kind::Kick;
	default:
		return Kind::Keep;
	}
}

/// A thing as a target to throw at: where it stands, how tall it is, and how it moves. A creature goes on along its
/// heading. A villager goes on by the step of its walk while its state is not a final state.
openblack::creature_throw::ThrowTarget ThrowTargetOf(const openblack::ecs::Registry& registry, entt::entity thing,
                                                     const glm::vec3& standing)
{
	using namespace openblack;
	using namespace openblack::ecs::components;
	using Target = creature_throw::ThrowTarget;
	Target target {.kind = Target::Kind::Thing,
	               .mapPosition = {map_coords::ToFixed(standing.x), map_coords::ToFixed(standing.z)},
	               .height = ecs::systems::object_measures::Height(registry, thing)};
	if (const auto* walk = registry.TryGet<const CreatureLocomotion>(thing);
	    walk != nullptr && registry.AllOf<Creature, Transform>(thing))
	{
		const auto& transform = registry.Get<const Transform>(thing);
		const auto ahead = -(transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
		target.kind = Target::Kind::Creature;
		target.position = transform.position;
		target.heading = walk->started ? walk->heading.Radians() : GameHeading::Of(ahead.x, ahead.z).Radians();
		target.speed = walk->speed;
	}
	else if (registry.AllOf<Villager>(thing))
	{
		target.kind = Target::Kind::Villager;
		if (const auto* hug = registry.TryGet<const WallHug>(thing))
		{
			target.step = hug->step;
		}
		if (const auto* action = registry.TryGet<const LivingAction>(thing);
		    action != nullptr && Locator::infoConstants::has_value())
		{
			const auto top = action->states.at(static_cast<size_t>(LivingAction::Index::Top));
			const auto& table = Locator::infoConstants::value().villagerStateTable;
			target.walking = top < table.size() && table.at(top).isFinalState == 0;
		}
	}
	return target;
}
} // namespace

using creature_desires::Desire;

namespace openblack::ecs::systems::mind_detail
{
constexpr float k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
constexpr float k_TurnsPerSecond = 1.0f / k_TurnSeconds;

/// The rows of the game's creature action table that seeing to each need counts as
constexpr std::string_view k_EatAction = "EatAfterExamining";
constexpr std::string_view k_DrinkAction = "DrinkFromTheSea";
constexpr std::string_view k_PooAction = "Poo";
constexpr std::string_view k_FartAction = "Fart";
constexpr std::string_view k_PukeAction = "Puke";
constexpr std::string_view k_SleepAction = "SleepOnTheSpot";
constexpr std::string_view k_ExamineAction = "ExamineByPickingUp";
constexpr std::string_view k_ThrowAboutAction = "PracticeThrow";
constexpr std::string_view k_HurlAction = "Hurl";

/// Just woken up, the eyelids droop and blink slowly; otherwise they are as they start
constexpr float k_SleepyOpenness = -0.7f;
constexpr int32_t k_SleepyBlinkIntervalMs = 2500;

void ApplyEyes(CreatureEyes* eyes, creature_mind::Eyes look)
{
	if (eyes == nullptr || look == creature_mind::Eyes::Unchanged)
	{
		return;
	}
	// Just woken up, only the lids and the blinking change
	if (look == creature_mind::Eyes::Sleepy)
	{
		eyes->openness = k_SleepyOpenness;
		eyes->blink.intervalMs = k_SleepyBlinkIntervalMs;
		return;
	}
	// The other looks change only how the eyes look: the lids and the blinking stay as they were
	eyes->mode = look == creature_mind::Eyes::Closed   ? creature_eyes::Mode::Closed
	             : look == creature_mind::Eyes::Stoned ? creature_eyes::Mode::Stoned
	             : look == creature_mind::Eyes::Wide   ? creature_eyes::Mode::Wide
	                                                   : creature_eyes::Mode::Calm;
}

/// The creature stops what it does: its eyes go calm, blink as usual, and its lids open by how rested it is
void RestEyes(const ecs::Registry& registry, entt::entity creature, CreatureEyes* eyes)
{
	if (eyes == nullptr)
	{
		return;
	}
	const auto* needs = registry.TryGet<const CreatureNeeds>(creature);
	eyes->mode = creature_eyes::Mode::Calm;
	eyes->blink.intervalMs = creature_eyes::k_BlinkIntervalMs;
	eyes->openness = creature_eyes::RestedOpenness(needs != nullptr ? needs->needs.exhaustion : 0.0f);
}

/// Sends the creature where its mind wants it to go; whether it can't go there and gives up what it is doing
/// Turning to face a point as a script asks, or down the slope, unless it faces it already
void FaceForScript(entt::entity creature, const creature_mind::Movement& move)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* self = registry.TryGet<const CreatureLocomotion>(creature);
	if (transform == nullptr || self == nullptr)
	{
		return;
	}
	const glm::vec2 from {transform->position.x, transform->position.z};
	auto point = move.point;
	if (move.kind == creature_mind::Movement::Kind::FaceDownSlope)
	{
		point = creature_mind::DownSlope(from, [](glm::vec2 at) -> std::optional<float> {
			if (!Locator::terrainSystem::has_value() || !map_coords::InBounds(glm::vec3(at.x, 0.0f, at.y)))
			{
				return std::nullopt;
			}
			return Locator::terrainSystem::value().GetHeightAt(at);
		});
	}
	const auto offset = point - from;
	const auto turn = creature_locomotion::HeadingOf(offset).TurnFrom(self->heading);
	// Down the slope it turns unless it stands on top of the place; to a script's point, unless it nearly faces it
	const bool turns = move.kind == creature_mind::Movement::Kind::FaceDownSlope
	                       ? glm::length(offset) >= creature_mind::k_OnTopOf
	                       : creature_mind::NeedsToTurnToFace(glm::length(offset), turn);
	if (turns)
	{
		Locator::creatureLocomotionSystem::value().TurnToFace(creature, point);
	}
}

/// A new agenda means that the creature stopped what it did. The creature of a player that is not the computer then
/// goes back to its walk speed. Other creatures keep the speed that they got last.
void KeepPaceWithAgenda(entt::entity creature, CreatureMindState& mind)
{
	if (mind.paceSerial == mind.idle.serial)
	{
		return;
	}
	mind.paceSerial = mind.idle.serial;
	const auto* body = Locator::entitiesRegistry::value().TryGet<const Creature>(creature);
	if (body == nullptr || !Locator::creatureLocomotionSystem::has_value())
	{
		return;
	}
	const bool computer = Locator::playerSystem::has_value() && Locator::playerSystem::value().IsComputerPlayer(body->owner);
	if (!creature_emergencies::KeepsRunSpeed(body->owner != PlayerNames::NEUTRAL, computer))
	{
		Locator::creatureLocomotionSystem::value().ResetPace(creature);
	}
}

bool Move(entt::entity creature, const creature_mind::Commands& commands)
{
	// A start that cuts the body also stops what the hands do, such as a catch or a let-go. The hand keeps what it holds.
	if (commands.cutBody && Locator::creatureObjectActionSystem::has_value())
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	if (!Locator::creatureLocomotionSystem::has_value())
	{
		return false;
	}
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	if (commands.stopMoving)
	{
		locomotion.Stop(creature);
	}
	if (!commands.move.has_value())
	{
		return false;
	}
	using Kind = creature_mind::Movement::Kind;
	using Pace = CreatureLocomotionSystemInterface::Pace;
	const auto& move = *commands.move;
	// A walk that does not ask for the run speed keeps the speed that the creature got last
	const auto pace = move.run ? Pace::Run : Pace::Keep;
	const auto object = move.object.has_value() ? std::optional(static_cast<entt::entity>(*move.object)) : std::nullopt;
	switch (move.kind)
	{
	case Kind::ToPoint:
	case Kind::Nearby:
	{
		const auto result = move.extraSpeed.has_value()
		                        ? locomotion.LeadTo(creature, move.point, *move.extraSpeed, move.maxDistance)
		                        : locomotion.MoveTo(creature, move.point, pace, move.minDistance, move.maxDistance);
		if (result == CreatureLocomotionSystemInterface::MoveResult::InvalidDestination && move.giveUpIfUnreachable)
		{
			return true;
		}
		break;
	}
	case Kind::ToObject:
		if (object.has_value())
		{
			locomotion.MoveToObject(creature, *object, pace, move.maxDistance);
		}
		break;
	case Kind::Follow:
		if (object.has_value())
		{
			locomotion.Follow(creature, *object, move.maxDistance, pace);
		}
		break;
	case Kind::FleeFrom:
		locomotion.FleeFrom(creature, move.point, pace);
		break;
	case Kind::TurnToFace:
		locomotion.TurnToFace(creature, move.point);
		break;
	case Kind::FacePoint:
	case Kind::FaceDownSlope:
		FaceForScript(creature, move);
		break;
	case Kind::GoNearObject:
	case Kind::GetAwayFromObject:
	case Kind::TurnToFaceObject:
	case Kind::TurnToFaceAwayFromObject:
	case Kind::ToThrowPosition:
		// Overseen turn by turn by the mind as it casts (StartSubMove)
		break;
	}
	return false;
}

bool IsSubMove(creature_mind::Movement::Kind kind)
{
	using Kind = creature_mind::Movement::Kind;
	return kind == Kind::GoNearObject || kind == Kind::GetAwayFromObject || kind == Kind::TurnToFaceObject ||
	       kind == Kind::TurnToFaceAwayFromObject || kind == Kind::ToThrowPosition;
}

/// The creature heals itself: its life becomes full, and its wounds and blood age as if much time went by
void HealItself(ecs::Registry& registry, entt::entity creature)
{
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
	{
		needs->needs.life = 1.0f;
	}
	if (Locator::creatureSkinSystem::has_value())
	{
		Locator::creatureSkinSystem::value().Heal(creature, creature_mind::k_HealItselfCounts);
	}
}

/// The rest of the agenda is no use: it gives it up
void GiveUp(creature_mind::IdleMind& idle)
{
	idle.step = idle.agenda.size();
	idle.stepStarted = false;
	idle.gaveUp = true;
}

/// The kiss of a creature. It kisses only when its species has the high and the low kiss animations. A kiss of the air
/// plays the low kiss alone. A kiss of another creature blends the low kiss towards the high kiss, so that the lips of
/// the two meet. When the lips cannot meet, it does not kiss.
void StartKiss(entt::entity creature, const creature_mind::Commands::KissOrder& kiss, CreatureAnimation& animation)
{
	if (!Locator::creatureAnimationSystem::has_value() || creature_layers::IsPlaying(animation.body))
	{
		return;
	}
	auto& bodies = Locator::creatureAnimationSystem::value();
	if (!bodies.AnimationDuration(creature, creature_kiss::k_HighKiss).has_value() ||
	    !bodies.AnimationDuration(creature, creature_kiss::k_LowKiss).has_value())
	{
		return;
	}
	float highWeight = 0.0f;
	if (kiss.partner.has_value())
	{
		const auto partner = static_cast<entt::entity>(*kiss.partner);
		const auto& registry = Locator::entitiesRegistry::value();
		const auto self = bodies.KissPointsOf(creature);
		const auto other = registry.Valid(partner) ? bodies.KissPointsOf(partner) : std::nullopt;
		const auto weight = self.has_value() && other.has_value() ? creature_kiss::HighWeight(*self, *other) : std::nullopt;
		if (!weight.has_value())
		{
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} cannot kiss creature {}: the lips do not meet",
			                    entt::to_integral(creature), *kiss.partner);
			return;
		}
		highWeight = *weight;
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} kisses creature {} with the high kiss at {:.3f}",
		                    entt::to_integral(creature), *kiss.partner, highWeight);
	}
	if (auto body = creature_layers::PlayOnce(animation.body, creature_kiss::k_LowKiss, false))
	{
		if (highWeight > 0.0f)
		{
			body->blendedWith = creature_kiss::k_HighKiss;
			body->blendWeight = highWeight;
		}
		// A kiss keeps its pace when the creature is told to hurry, and it gets to its moment at the kiss time
		body->keepsPace = true;
		body->kiss = true;
		animation.body = *body;
	}
}

/// Whether the creature's species has the clip. A body that is not loaded yet counts as one that has every clip.
bool HasClip(entt::entity creature, const CreatureAnimation& animation, size_t clip)
{
	if (!Locator::creatureAnimationSystem::has_value() || animation.skeleton.Empty())
	{
		return true;
	}
	return Locator::creatureAnimationSystem::value().AnimationDuration(creature, clip).has_value();
}

/// Whether the body does not have a clip that the next step plays as it starts. A step that started has no test.
bool ClipMissing(entt::entity creature, const CreatureAnimation& animation, const creature_mind::IdleMind& mind,
                 std::optional<size_t> communication)
{
	if (mind.stepStarted || mind.step >= mind.agenda.size())
	{
		return false;
	}
	return creature_mind::LacksClipToStart(mind.agenda[mind.step], communication,
	                                       [creature, &animation](size_t clip) { return HasClip(creature, animation, clip); });
}

void Apply(entt::entity creature, const creature_mind::Commands& commands, CreatureAnimation& animation, CreatureEyes* eyes)
{
	if (commands.endSit || commands.endStaticLoop)
	{
		animation.body = creature_layers::EndLoop(animation.body);
	}
	if (commands.endDance)
	{
		animation.body = creature_layers::EndDance(animation.body);
	}
	if (commands.startDance.has_value())
	{
		if (auto body = creature_layers::PlayDance(animation.body, *commands.startDance))
		{
			animation.body = *body;
		}
		else
		{
			NoteRefused(creature, creature_animation_log::Layer::Body, creature_layers::animations::k_DanceStart,
			            creature_layers::CurrentAnimation(animation.body));
		}
	}
	if (commands.playOnce.has_value())
	{
		if (auto body = creature_layers::PlayOnce(animation.body, *commands.playOnce, commands.mirrored))
		{
			animation.body = *body;
		}
		else
		{
			NoteRefused(creature, creature_animation_log::Layer::Body, *commands.playOnce,
			            creature_layers::CurrentAnimation(animation.body));
		}
	}
	if (commands.startSequence.has_value())
	{
		const auto& sequence = *commands.startSequence;
		// A start that cuts the body plays from a body that stands
		const auto from = commands.cutBody ? creature_layers::BodyAction {} : animation.body;
		if (auto body = creature_layers::PlaySequence(from, sequence[0], sequence[1], sequence[2], commands.holdLoop,
		                                              commands.mirrored))
		{
			body->keepsPace = commands.keepsPace;
			animation.body = *body;
		}
		else
		{
			NoteRefused(creature, creature_animation_log::Layer::Body, sequence[0], creature_layers::CurrentAnimation(from));
		}
	}
	if (commands.playOverlay.has_value())
	{
		if (auto gesture = creature_layers::PlayGesture(animation.gesture, *commands.playOverlay))
		{
			animation.gesture = *gesture;
		}
		else
		{
			NoteRefused(creature, creature_animation_log::Layer::Gesture, *commands.playOverlay,
			            animation.gesture.animation.value_or(creature_layers::animations::k_Stand));
		}
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (commands.relaxFace)
	{
		shown_creature_faces::ChangeFace(registry, creature, animation.face, [](const creature_layers::FaceLayer& face) {
			return creature_layers::RelaxFace(face);
		});
	}
	if (const auto& face = commands.face; face.has_value())
	{
		shown_creature_faces::ChangeFace(registry, creature, animation.face, [&face](const creature_layers::FaceLayer& layer) {
			return creature_layers::PullFace(layer, creature_face::AnimationOf(face->face), face->milliseconds, face->cue);
		});
	}
	ApplyEyes(eyes, commands.eyes);
	if (eyes != nullptr && commands.blinkIntervalMs.has_value())
	{
		eyes->blink.intervalMs = *commands.blinkIntervalMs;
	}
}

/// What the creature shows when it communicates its state now
creature_desires::Communication Communication(const CreatureMindState& mind)
{
	if (!mind.desires.has_value())
	{
		return {};
	}
	const auto turnsSinceFeedback =
	    mind.feedbackSeconds.has_value()
	        ? std::optional(static_cast<uint32_t>(std::lround(*mind.feedbackSeconds / k_TurnSeconds)))
	        : std::nullopt;
	return creature_desires::Communicate(*mind.desires, turnsSinceFeedback,
	                                     static_cast<uint32_t>(std::lround(k_TurnsPerSecond)), mind.attitudeToPlayer);
}

/// After the creature communicates its state, its desire to show its state cannot grow for a time, and the plan found
/// for that desire is forgotten
void Communicated(CreatureMindState& mind)
{
	if (mind.desires.has_value())
	{
		creature_desires::Suppress(*mind.desires, Desire::ManifestState, creature_desires::k_CommunicateSuppressSeconds,
		                           k_TurnsPerSecond);
	}
	mind.planner.best.at(static_cast<size_t>(Desire::ManifestState)).reset();
}

/// What a step did to the body. A meal of a plan decreases the desire of the plan only when the plan ends, as all
/// actions of plans do.
void TakeEffect(entt::entity creature, const creature_mind::Commands& commands, creature_desires::Desires& desires,
                bool planned)
{
	using creature_mind::Effect;
	if (commands.effect == Effect::None || !Locator::creaturePhysiologySystem::has_value())
	{
		return;
	}
	auto& physiology = Locator::creaturePhysiologySystem::value();
	switch (commands.effect)
	{
	case Effect::Eat:
		// The meal decreases the hunger when the creature swallows it. A meal from no plan also decreases the desire by
		// the value in the action table.
		if (!planned)
		{
			Satisfied(creature, desires, k_EatAction);
		}
		break;
	case Effect::Examined:
		Satisfied(creature, desires, k_ExamineAction);
		break;
	case Effect::ThrewAbout:
		Satisfied(creature, desires, k_ThrowAboutAction);
		break;
	case Effect::Hurled:
		Satisfied(creature, desires, k_HurlAction);
		break;
	case Effect::Drink:
		physiology.Drink(creature);
		Satisfied(creature, desires, k_DrinkAction);
		// The creature does not want water for a time, and its fire state cools
		if (const auto temperature = creature_drink::Drank(desires, k_TurnsPerSecond); Locator::fireSystem::has_value())
		{
			Locator::fireSystem::value().SetTemperature(creature, temperature, entt::null);
		}
		break;
	case Effect::Poo:
		// At the end of the squat, the bowels are empty. The lumps dropped during the squat stopped the desire to poo
		physiology.EmptyBowels(creature);
		Satisfied(creature, desires, k_PooAction);
		break;
	case Effect::Fart:
		// At the end of the squat, the bowels are empty. The action table sets how much the desire to poo decreases
		physiology.EmptyBowels(creature);
		Satisfied(creature, desires, k_FartAction);
		break;
	case Effect::Puke:
		Satisfied(creature, desires, k_PukeAction);
		break;
	case Effect::Slept:
		Satisfied(creature, desires, k_SleepAction);
		break;
	case Effect::CameRound:
		physiology.WakeFromFaint(creature);
		break;
	case Effect::Revived:
		magic_living::SetLife(creature, creature_odd::k_StrangeReviveLife);
		break;
	case Effect::RanAway:
	case Effect::Completed:
	case Effect::Observed:
	case Effect::Inspected:
	case Effect::TookFromPile:
	case Effect::TookFromHand:
	case Effect::Stomped:
	case Effect::None:
		break;
	}
}

/// A step saying the plan's action counts as done there, before the step that finishes it, sees to its desire
void CountCompleted(entt::entity creature, CreatureMindState& mind, const creature_mind::Commands& commands,
                    const creature_mind_tables::Tables* tables)
{
	if (commands.effect != creature_mind::Effect::Completed || !mind.planActive || !mind.planner.current.has_value() ||
	    tables == nullptr || !mind.desires.has_value() || mind.planner.current->action >= tables->actions.size())
	{
		return;
	}
	Satisfied(creature, *mind.desires, tables->actions[mind.planner.current->action].name);
}

/// The town nearest a point along the ground, of any player or none. The distance is the table distance between map
/// positions
entt::entity NearestTown(const ecs::Registry& registry, glm::vec3 point)
{
	entt::entity nearest = entt::null;
	auto best = std::numeric_limits<float>::max();
	// Each player's town list in turn and the neutral list last: the first met wins a tie
	for (const auto entity : ecs::player_towns::AllInOrder(registry))
	{
		const auto* at = registry.TryGet<const Transform>(entity);
		if (at == nullptr)
		{
			continue;
		}
		// The table distance between the two map positions
		const float distance = gutils::GetDistanceInMetres(point, at->position);
		if (distance < best)
		{
			best = distance;
			nearest = entity;
		}
	}
	return nearest;
}

/// Put a new handful for a creature into the map at once, as the game does with each new pot. A creature cannot pick
/// up a thing that is not in the map.
void PutOnTheMap(entt::entity food)
{
	if (Locator::entitiesMap::has_value())
	{
		Locator::entitiesMap::value().Refile(food);
	}
}

/// The food a creature fishing brings out of the sea: a handful at its feet of a thousand for each unit of its size,
/// drawn half as big again as the creature, belonging to the nearest town. Nothing comes out of a fish farm.
std::optional<entt::entity> FoodFromTheSea(ecs::Registry& registry, entt::entity creature)
{
	constexpr float k_FoodPerSize = 1000.0f;
	constexpr float k_ScalePerSize = 1.5f;
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	const float size = body != nullptr ? body->size : 1.0f;
	const auto amount = static_cast<int32_t>(size * k_FoodPerSize);
	const auto food = ecs::archetypes::PotArchetype::Create(transform->position, 0.0f, PotInfo::WheatInHand, amount,
	                                                        NearestTown(registry, transform->position));
	if (food == entt::null)
	{
		return std::nullopt;
	}
	if (auto* foodTransform = registry.TryGet<Transform>(food))
	{
		foodTransform->scale = glm::vec3(size * k_ScalePerSize);
	}
	PutOnTheMap(food);
	return food;
}

/// The resource a pot of a kind holds, by the game's pot table
ResourceType ResourceTypeOf(PotInfo kind)
{
	const auto row = static_cast<size_t>(kind);
	return Locator::infoConstants::has_value() && row < Locator::infoConstants::value().pot.size()
	           ? Locator::infoConstants::value().pot.at(row).resourceType
	           : ResourceType::Food;
}

/// The food that a creature takes from a thing (a pile, the food pile of a storage pit, a field or a tree). It is a
/// handful of wheat or fruit, of 1000 for each unit of the size of the creature, but not more than the store of food of
/// the thing. Its scale is 3.75 times the size of the creature, and it belongs to the nearest town. A field and a tree
/// have no store of food, thus their handful holds no food.
std::optional<entt::entity> FoodFromPile(ecs::Registry& registry, entt::entity creature, entt::entity pile,
                                         creature_mind::ObjectOrder::Handful handful)
{
	constexpr float k_ScalePerSize = 3.75f;
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (transform == nullptr || !registry.Valid(pile) || !Locator::resourceStoreSystem::has_value())
	{
		return std::nullopt;
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	const float size = body != nullptr ? body->size : 1.0f;
	const auto kind = creature_town_help::HandfulPot(handful);
	const auto held = Locator::resourceStoreSystem::value().GetResource(pile, ResourceTypeOf(kind));
	const auto amount = creature_meals::HandfulAmount(size, held);
	// The handful appears on the ground beside the creature, most of its reach towards the pile, at the creature's own
	// height
	glm::vec3 at = transform->position;
	const auto* pileTransform = registry.TryGet<const Transform>(pile);
	const auto reach = Locator::creatureObjectActionSystem::has_value()
	                       ? Locator::creatureObjectActionSystem::value().PickUpStraightReach(creature)
	                       : std::nullopt;
	if (pileTransform != nullptr && reach.has_value())
	{
		const auto point = creature_reach::HandfulPoint({transform->position.x, transform->position.z},
		                                                {pileTransform->position.x, pileTransform->position.z}, *reach);
		at = {point.x, transform->position.y, point.y};
	}
	// Make the handful also when it holds no food, as from a field or a tree
	const auto food = ecs::archetypes::PotArchetype::CreateEmpty(at, 0.0f, kind);
	if (food == entt::null)
	{
		return std::nullopt;
	}
	auto& pot = registry.Get<Pot>(food);
	pot.amount = amount;
	pot.town = NearestTown(registry, transform->position);
	if (auto* foodTransform = registry.TryGet<Transform>(food))
	{
		foodTransform->scale = glm::vec3(size * k_ScalePerSize);
	}
	PutOnTheMap(food);
	return food;
}

/// Tells the creature's hands what to do with a thing
void Order(ecs::Registry& registry, entt::entity creature, const creature_mind::Commands& commands)
{
	if (!commands.object.has_value() || !Locator::creatureObjectActionSystem::has_value())
	{
		return;
	}
	auto& hands = Locator::creatureObjectActionSystem::value();
	const auto& order = *commands.object;
	const auto object = order.object.has_value() ? std::optional(static_cast<entt::entity>(*order.object)) : std::nullopt;
	const auto ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(order.point) : 0.0f;
	const glm::vec3 point {order.point.x, ground, order.point.y};
	using Kind = creature_mind::ObjectOrder::Kind;
	switch (order.kind)
	{
	case Kind::PickUp:
		if (object.has_value() && registry.Valid(*object))
		{
			hands.PickUp(creature, *object);
		}
		else
		{
			hands.Cancel(creature);
		}
		break;
	case Kind::PutDown:
		hands.PutDown(creature);
		break;
	case Kind::Discard:
		if (order.animation != 0)
		{
			hands.DiscardWith(creature, order.animation);
		}
		else
		{
			hands.Discard(creature);
		}
		break;
	case Kind::Eat:
		if (order.swallows)
		{
			hands.EatHeld(creature);
		}
		else
		{
			hands.EatHeldOnly(creature);
		}
		break;
	case Kind::Keep:
		hands.Keep(creature, order.animation);
		break;
	case Kind::Throw:
	case Kind::ThrowNearby:
		// At the place of the object as the throw starts, when the order says so. The place of a thing that a creature
		// holds is the place of that creature.
		if (order.atObject && object.has_value() && registry.Valid(*object))
		{
			const auto* held = registry.TryGet<const HeldByCreature>(*object);
			const auto place = held != nullptr && registry.Valid(held->creature) ? held->creature : *object;
			const auto* at = registry.TryGet<const Transform>(place);
			const auto* self = registry.TryGet<const Transform>(creature);
			if (at != nullptr && self != nullptr && Locator::terrainSystem::has_value())
			{
				// The game aims ahead of a thing that moves, by the time that the throw takes to get there
				const auto& island = Locator::terrainSystem::value();
				const auto aim = creature_throw::AimAt(self->position, ThrowTargetOf(registry, *object, at->position),
				                                       [&island](int32_t x, int32_t z) { return island.GetAltitude(x, z); });
				hands.ThrowAimed(creature, aim.point, aim.seconds);
				break;
			}
		}
		// At a point in the air when it has a height, as the player's camera; otherwise at the ground there
		hands.Throw(creature, {point.x, order.pointHeight.value_or(point.y), point.z});
		break;
	case Kind::TakeFromHand:
		if (!object.has_value() || !registry.Valid(*object) || !hands.TakeFromHand(creature, *object, order.mirrored))
		{
			hands.Cancel(creature);
		}
		break;
	case Kind::Destroy:
		if (object.has_value() && registry.Valid(*object))
		{
			hands.Destroy(creature, *object);
		}
		else
		{
			hands.Cancel(creature);
		}
		break;
	case Kind::Kick:
		if (!object.has_value() || !registry.Valid(*object) || !hands.Kick(creature, *object))
		{
			hands.Cancel(creature);
		}
		break;
	case Kind::PointAt:
		if (object.has_value() || order.seconds.has_value() || order.pointHeight.has_value())
		{
			// At a thing, wherever it is now and goes; or at a point in the air, as the camera
			const auto* at =
			    object.has_value() && registry.Valid(*object) ? registry.TryGet<const Transform>(*object) : nullptr;
			if (object.has_value() && at == nullptr)
			{
				hands.Cancel(creature);
				break;
			}
			const auto aim = at != nullptr ? at->position : glm::vec3 {point.x, order.pointHeight.value_or(point.y), point.z};
			hands.PointAtFor(creature, aim, order.seconds.value_or(creature_object_actions::k_PointSeconds), object);
			break;
		}
		hands.PointAt(creature, point);
		break;
	case Kind::Catch:
		if (!object.has_value() || !registry.Valid(*object) || !hands.Catch(creature, *object))
		{
			hands.Cancel(creature);
		}
		break;
	case Kind::ThrowInStore:
	{
		// At the store's own place on the ground, taking as long to fly there as it would take to fall the distance
		// along the ground
		const auto* store = object.has_value() && registry.Valid(*object) ? registry.TryGet<const Transform>(*object) : nullptr;
		const auto* self = registry.TryGet<const Transform>(creature);
		if (store == nullptr || self == nullptr)
		{
			hands.Cancel(creature);
			break;
		}
		const glm::vec2 at {store->position.x, store->position.z};
		const auto height = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(at) : 0.0f;
		const auto flight = creature_throw::FlightTime(self->position, {at.x, height, at.y});
		hands.ThrowTaking(creature, {at.x, height, at.y}, flight);
		break;
	}
	case Kind::TakeFromPile:
		if (const auto food = object.has_value() ? FoodFromPile(registry, creature, *object, order.handful) : std::nullopt;
		    food.has_value())
		{
			if (auto* mind = registry.TryGet<CreatureMindState>(creature))
			{
				mind->takenFood = *food;
				mind->takenFrom = *object;
			}
			hands.PickUp(creature, *food);
		}
		else
		{
			hands.Cancel(creature);
		}
		break;
	case Kind::FishFromSea:
		if (const auto food = FoodFromTheSea(registry, creature); food.has_value())
		{
			hands.PickUp(creature, *food);
		}
		else
		{
			hands.Cancel(creature);
		}
		break;
	}
}

/// Remove the food of the handful from the pile that the creature took it from. Only a pile loses food: a field or a
/// tree loses nothing.
void GiveUpTakenFood(ecs::Registry& registry, CreatureMindState& mind)
{
	const auto food = std::exchange(mind.takenFood, entt::null);
	const auto pile = std::exchange(mind.takenFrom, entt::null);
	const auto* pot = registry.Valid(food) ? registry.TryGet<const Pot>(food) : nullptr;
	if (pot == nullptr || !registry.Valid(pile) || !registry.AllOf<Pot>(pile) || !Locator::resourceStoreSystem::has_value())
	{
		return;
	}
	static_cast<void>(Locator::resourceStoreSystem::value().RemoveResource(pile, ResourceType::Food, pot->amount));
}

/// Whether the creature holds a thing in its left hand
bool HeldLeft(const ecs::Registry& registry, entt::entity creature)
{
	const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
	return held != nullptr && held->mirrored;
}

creature_mind::HandsState HandsOf(entt::entity creature)
{
	if (!Locator::creatureObjectActionSystem::has_value())
	{
		return creature_mind::HandsState::Idle;
	}
	using State = CreatureObjectActionSystemInterface::State;
	switch (Locator::creatureObjectActionSystem::value().GetState(creature))
	{
	case State::Busy:
		return creature_mind::HandsState::Busy;
	case State::Done:
		return creature_mind::HandsState::Done;
	case State::Failed:
		return creature_mind::HandsState::Failed;
	case State::Idle:
		break;
	}
	return creature_mind::HandsState::Idle;
}
/// The creature's plan has it fight another creature: whether the other creature agreed and the fight started. When
/// agreed is true, the other creature agreed before and is not asked again.
CreatureFightSystemInterface::StartResult StartPlannedFight(entt::entity creature, entt::entity opponent, bool agreed)
{
	using Result = CreatureFightSystemInterface::StartResult;
	return Locator::creatureFightSystem::has_value()
	           ? Locator::creatureFightSystem::value().StartPlannedFight(creature, opponent, agreed)
	           : Result::NoOpponent;
}
} // namespace openblack::ecs::systems::mind_detail

using namespace openblack::ecs::systems::mind_detail;

namespace openblack::ecs::systems::mind_detail
{
bool Thinks(const ecs::Registry& registry, entt::entity creature)
{
	return registry.Valid(creature) && registry.AllOf<Creature, CreatureMindState, CreatureAnimation, Transform>(creature);
}
} // namespace openblack::ecs::systems::mind_detail

void CreatureMindSystem::AgendaTurn(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const bool acts = std::erase(_acting, entity) > 0 && Thinks(registry, entity);
	// The step of the agenda, only for a mind that thought on in this turn
	const auto act = [&]() {
		const auto& creature = registry.Get<const Creature>(entity);
		auto& mind = registry.Get<CreatureMindState>(entity);
		auto& animation = registry.Get<CreatureAnimation>(entity);
		const auto& transform = registry.Get<const Transform>(entity);
		const auto random = [this](uint32_t range) { return _random.GameRand(range); };
		// The agendas, the faces and the sides draw on the same synced seed of the game
		const auto& gameRandom = random;
		// Whether the cinema bars are in for a cut scene, and which script task brought them
		const bool barsIn =
		    Locator::cinematicDirectorSystem::has_value() && Locator::cinematicDirectorSystem::value().IsWideScreenOn();
		const auto barsOwner = barsIn ? Locator::cinematicDirectorSystem::value().GetWideScreenOwner() : 0u;
		const auto* lookTables = GetTables();
		const auto cutSceneLook = lookTables != nullptr
		                              ? creature_mind_tables::FindAction(*lookTables, creature_mind::k_CutSceneLookAction)
		                              : std::nullopt;
		auto* needs = registry.TryGet<CreatureNeeds>(entity);
		const bool scripted = registry.AllOf<ScriptControlled>(entity);
		auto* eyes = registry.TryGet<CreatureEyes>(entity);
		// A script's cut scene makes every creature free to think for itself look at the camera, again and again
		// until the cut scene is over; what it is doing then plays out to its end
		if (!scripted && cutSceneLook.has_value())
		{
			const bool looking =
			    mind.planActive && mind.planner.current.has_value() && mind.planner.current->action == *cutSceneLook;
			if (creature_mind::StartsLookAtCameraInCutScene(barsIn, barsOwner, looking))
			{
				ForcePlan(entity, {.desire = creature_desires::Desire::ObeyPlayer,
				                   .action = creature_mind::k_CutSceneLookAction,
				                   .object = entity,
				                   .ordered = true,
				                   .fightBodyEndsAgain = true});
			}
		}
		// When the creature examines a different creature, it finds the desires of the two creatures each turn.
		// It does not do this on the first turn.
		if (!scripted && mind.idle.stepStarted && mind.idle.step < mind.idle.agenda.size() &&
		    mind.idle.agenda[mind.idle.step].kind == creature_mind::Step::Kind::Inspect &&
		    mind.idle.agenda[mind.idle.step].object.has_value())
		{
			WeighWatchedCreature(entity, mind, static_cast<entt::entity>(*mind.idle.agenda[mind.idle.step].object));
		}
		// Shut in, the creature clears a thing out of its way and walks on. With nothing to clear, its walk ends where
		// it stands. When the search finds no route at all, the creature gives up what it does and makes a sound of
		// refusal. It also leaves the thing that it went to alone for a time.
		StepClearingWay(entity, mind);
		if (mind.idle.step < mind.idle.agenda.size())
		{
			const auto& step = mind.idle.agenda[mind.idle.step];
			auto* walk = registry.TryGet<CreatureLocomotion>(entity);
			const bool walkToPoint = step.kind == creature_mind::Step::Kind::Move && !step.movement.object.has_value() &&
			                         step.movement.giveUpIfUnreachable &&
			                         (step.movement.kind == creature_mind::Movement::Kind::ToPoint ||
			                          step.movement.kind == creature_mind::Movement::Kind::Nearby);
			const bool walkToObject = (step.kind == creature_mind::Step::Kind::Move ||
			                           (step.kind == creature_mind::Step::Kind::FollowWatching && mind.idle.followMoved)) &&
			                          step.movement.object.has_value();
			// Any walk that stops on the first turn of its search can clear the way, one time for each stop. This
			// includes the walk of the hands to a thing that they pick up, stomp on or kick.
			const bool handsStep = step.kind == creature_mind::Step::Kind::Object;
			const bool anyWalk = step.kind == creature_mind::Step::Kind::Move || walkToObject || handsStep;
			std::optional<CreatureLocomotion::GaveUp> gaveUp;
			if (walk != nullptr && mind.idle.stepStarted && anyWalk)
			{
				gaveUp = std::exchange(walk->gaveUp, std::nullopt);
			}
			if (gaveUp.has_value() && ClearWay(entity, mind, gaveUp->destination, gaveUp->met))
			{
				// The new plan stops what the hands did
				if (handsStep && Locator::creatureObjectActionSystem::has_value())
				{
					Locator::creatureObjectActionSystem::value().Cancel(entity);
				}
				StepClearingWay(entity, mind);
			}
			else if (walk != nullptr && walk->failed && mind.idle.stepStarted && (walkToPoint || walkToObject))
			{
				if (const auto object = step.movement.object)
				{
					mind.beliefs.MarkBlocked(static_cast<entt::entity>(*object), mind.turn);
				}
				Abandon(mind);
				mind.idle.step = mind.idle.agenda.size();
				mind.idle.stepStarted = false;
				mind.idle.gaveUp = true;
				RefuseNoRoute(entity);
			}
			// After a pick-up, a stomp or a kick that found no way to its thing, the creature does not choose the thing
			// again for a time
			else if (step.kind == creature_mind::Step::Kind::Object && mind.idle.stepStarted &&
			         Locator::creatureObjectActionSystem::has_value() && HandsOf(entity) == creature_mind::HandsState::Failed)
			{
				const auto failure = Locator::creatureObjectActionSystem::value().GetFailure(entity);
				if (step.order.object.has_value() &&
				    creature_object_actions::LeavesThingAlone(HandsKindOf(step.order.kind), failure))
				{
					mind.beliefs.MarkBlocked(static_cast<entt::entity>(*step.order.object), mind.turn);
				}
				if (creature_object_actions::RefusesAloud(failure))
				{
					RefuseNoRoute(entity);
				}
			}
		}
		const bool moving =
		    Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(entity);
		if (mind.idle.step < mind.idle.agenda.size())
		{
			++mind.stepTurns;
		}
		StepSubMove(entity, creature_layers::IsPlaying(animation.body));
		// A drawing that a step started continues each turn; a dance continues its own only on the turns of its move
		if (const auto* drawing = registry.TryGet<const CreatureGestureDrawing>(entity);
		    drawing != nullptr && !drawing->inDance)
		{
			StepGesture(entity);
		}
		// A catch takes up the body until it is over, but the plan to catch is carried out as soon as the catch starts
		const bool catching =
		    Locator::creatureObjectActionSystem::has_value() && Locator::creatureObjectActionSystem::value().IsCatching(entity);
		// An action that plays on after its plan stopped also keeps the body busy: the new plan waits until it ends
		const bool playsOn = Locator::creatureObjectActionSystem::has_value() &&
		                     Locator::creatureObjectActionSystem::value().PlaysOnAlone(entity);
		const bool catchStep = mind.idle.stepStarted && mind.idle.step < mind.idle.agenda.size() &&
		                       mind.idle.agenda[mind.idle.step].kind == creature_mind::Step::Kind::Object &&
		                       mind.idle.agenda[mind.idle.step].order.kind == creature_mind::ObjectOrder::Kind::Catch;
		// As a step that communicates its state starts, the creature finds what it feels at that time
		const bool communicates = !mind.idle.stepStarted && mind.idle.step < mind.idle.agenda.size() &&
		                          mind.idle.agenda[mind.idle.step].kind == creature_mind::Step::Kind::Communicate;
		const auto communication = communicates ? Communication(mind) : creature_desires::Communication {};
		const auto inHand = Locator::creatureObjectActionSystem::has_value()
		                        ? Locator::creatureObjectActionSystem::value().GetHeld(entity)
		                        : std::nullopt;
		const auto [cameraFocus, cameraFocusAhead] = CameraFocusOf(registry, entity, mind.idle);
		const creature_mind::Senses senses {
		    .seconds = k_TurnSeconds,
		    .bodyBusy = creature_layers::IsPlaying(animation.body) || moving || playsOn,
		    .bodyLooping = creature_layers::IsLooping(animation.body),
		    .clipMissing = ClipMissing(entity, animation, mind.idle, communication.animation),
		    .realTime = animation.realTime,
		    .bodyDancing = creature_layers::IsDancing(animation.body),
		    .moving = moving,
		    .onlyMoves = moving && !creature_layers::IsPlaying(animation.body) && !playsOn,
		    .subMove = SubMoveOf(entity),
		    .position = glm::vec2(transform.position.x, transform.position.z),
		    .communication = communication,
		    .rested = needs != nullptr && needs->rested,
		    .better = needs != nullptr && creature_mind::IsBetter(needs->needs.life, needs->needs.exhaustion),
		    .hands = catchStep && catching ? creature_mind::HandsState::Done : HandsOf(entity),
		    .handsTouched = Locator::creatureObjectActionSystem::has_value() &&
		                    Locator::creatureObjectActionSystem::value().HandsTouched(entity),
		    .held = inHand.has_value() ? std::optional(entt::to_integral(*inHand)) : std::nullopt,
		    .heldLeft = HeldLeft(registry, entity),
		    .objectInMap = WaitedForInMap(registry, mind.idle),
		    .objectMoving = WaitedForMoving(registry, mind.idle),
		    .objectBusy = WaitedForBusy(registry, mind.idle),
		    .objectInTheAir = WatchedIsInTheAir(registry, mind.idle),
		    .objectFlying = StillObjectFlies(registry, mind.idle),
		    .feelings = FeelingsOf(mind),
		    .camera = CameraViewOf(registry, entity, mind.idle),
		    .hand = HandViewOf(registry, entity, mind.idle),
		    .swap = SwapViewOf(entity),
		    .overlayPlaying = animation.gesture.animation.has_value(),
		    .bodyPlaying = creature_layers::IsPlaying(animation.body),
		    .dance = DanceViewOf(entity, mind.idle),
		    .followed = FollowedViewOf(registry, transform.position, mind),
		    .cameraFocus = cameraFocus,
		    .cameraFocusAhead = cameraFocusAhead,
		    .followHand = FollowHandViewOf(registry, entity, mind, creature_layers::IsPlaying(animation.body)),
		    .watchPlayer = WatchPlayerViewOf(registry, entity, mind, creature_layers::IsPlaying(animation.body)),
		    .fighting = registry.AllOf<CreatureFighting>(entity),
		    .totemShare = mind_town_help::TotemShareOf(registry, mind.idle),
		};
		const auto commands = creature_mind::Think(mind.idle, senses, random, gameRandom);
		NoteStepSeen(entity, mind);
		if (commands.communicated.has_value())
		{
			SPDLOG_LOGGER_DEBUG(
			    spdlog::get("game"), "Creature {} shows its state: desire {}, animation {}", entt::to_integral(entity),
			    commands.communicated->desire.has_value() ? static_cast<int>(*commands.communicated->desire) : -1,
			    commands.communicated->animation.has_value() ? static_cast<int64_t>(*commands.communicated->animation) : -1);
			Communicated(mind);
		}
		if (commands.dance.has_value())
		{
			CarryOutDance(entity, mind.idle, *commands.dance);
		}
		KeepDanceWithAgenda(entity, mind.idle);
		KeepPaceWithAgenda(entity, mind);
		if (commands.restEyes)
		{
			RestEyes(registry, entity, eyes);
		}
		if (commands.kiss.has_value())
		{
			StartKiss(entity, *commands.kiss, animation);
		}
		Apply(entity, commands, animation, eyes);
		if (Move(entity, commands))
		{
			GiveUp(mind.idle);
		}
		if (commands.move.has_value() && IsSubMove(commands.move->kind) && mind.idle.step < mind.idle.agenda.size())
		{
			StartSubMove(entity, *commands.move, mind.idle.agenda[mind.idle.step].seconds);
		}
		if (commands.gesture.has_value() && !StartGesture(entity, *commands.gesture, commands.gestureInDance) &&
		    commands.gestureInDance)
		{
			StepGesture(entity);
		}
		if (commands.endPoint)
		{
			EndDanceGesturePoint(entity);
		}
		if (commands.releaseCast)
		{
			ReleaseCast(entity);
		}
		// The miracle is tried as the casting pose's loop begins; the rest of the agenda is no use without it
		if (commands.cast.has_value() &&
		    ((commands.cast->place.has_value() &&
		      !TryMiracleAt(entity, static_cast<MagicType>(commands.cast->magicType), *commands.cast->place)) ||
		     (!commands.cast->place.has_value() && commands.cast->object.has_value() &&
		      !TryMiracle(entity, static_cast<MagicType>(commands.cast->magicType),
		                  static_cast<entt::entity>(*commands.cast->object)))))
		{
			mind.idle.step = mind.idle.agenda.size();
			mind.idle.stepStarted = false;
			mind.idle.gaveUp = true;
		}
		Order(registry, entity, commands);
		// It asks the other creature to fight. If the other creature does not agree, the rest of the agenda is no use.
		// When the other creature agrees while a move of a fight that stopped still finishes, the step waits. It starts
		// again on the next turn and keeps the answer: the other creature is asked one time only.
		if (commands.startFight.has_value())
		{
			using Result = CreatureFightSystemInterface::StartResult;
			const creature_mind::IdleMind::StepRef here {.serial = mind.idle.serial, .step = mind.idle.step};
			const auto& kept = mind.idle.fightAgreed;
			const bool agreed = kept.has_value() && kept->serial == here.serial && kept->step == here.step;
			const auto started = StartPlannedFight(entity, static_cast<entt::entity>(*commands.startFight), agreed);
			if (started == Result::Finishing)
			{
				mind.idle.stepStarted = false;
				mind.idle.fightAgreed = here;
			}
			else if (started != Result::Started)
			{
				mind.idle.step = mind.idle.agenda.size();
				mind.idle.stepStarted = false;
				mind.idle.gaveUp = true;
			}
		}
		// A joint action: when the other creature refuses to be its partner, the rest of the agenda is no use
		if (commands.requestPartner.has_value() &&
		    !RequestPartner(entity, static_cast<entt::entity>(commands.requestPartner->creature),
		                    commands.requestPartner->kind))
		{
			GiveUp(mind.idle);
		}
		if (commands.order.has_value() && commands.order->step != nullptr)
		{
			TakeOrder(static_cast<entt::entity>(commands.order->partner), entity, *commands.order->step);
		}
		if (commands.demobilise.has_value())
		{
			SetDemobilised(static_cast<entt::entity>(commands.demobilise->first), commands.demobilise->second);
		}
		// A joint action with a town ends in a desire about the town: its plan takes the place of the agenda. The
		// action stops as a failure, and so the joint action stops at once for the partner too, before the partner
		// takes up the desire of its own step.
		if (commands.applyDesire.has_value())
		{
			EndJointAction(entity);
			ForceDesireOn(entity, commands.applyDesire->desire, static_cast<entt::entity>(commands.applyDesire->thing));
		}
		if (commands.letGo.has_value())
		{
			LetGoOfHeld(entity, mind, *commands.letGo);
		}
		StepSwap(entity, commands);
		// A rock or a piece of a building breaks in two at once. Nothing else can, and the rest of the agenda is no use.
		if (commands.shatter.has_value())
		{
			const auto rock = static_cast<entt::entity>(*commands.shatter);
			if (registry.Valid(rock) && object_physics::IsRockOrPiece(rock) && Locator::dynamicsSystem::has_value())
			{
				object_physics::SplitRock(Locator::dynamicsSystem::value(), rock);
			}
			else
			{
				GiveUp(mind.idle);
			}
		}
		// The creature asks a teleport stone for a jump. A refused jump stops the plan as a failure. A jump gives the
		// creature the plan of the stone that carries it.
		if (commands.teleport.has_value())
		{
			const bool jumped =
			    Locator::teleportSystem::has_value() &&
			    Locator::teleportSystem::value().DoTeleport(static_cast<entt::entity>(*commands.teleport), entity, false);
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} asks the teleport stone {} for a jump: {}",
			                    entt::to_integral(entity), *commands.teleport, jumped ? "it jumps" : "refused");
			if (!jumped)
			{
				GiveUp(mind.idle);
			}
		}
		if (commands.carry != creature_mind::Commands::Carry::None)
		{
			using Carry = TeleportSystemInterface::Carry;
			const bool start = commands.carry == creature_mind::Commands::Carry::Start;
			auto carried = Carry::Done;
			if (Locator::teleportSystem::has_value())
			{
				auto& teleports = Locator::teleportSystem::value();
				carried = start ? (teleports.StartCarry(entity) ? Carry::Carried : Carry::Done) : teleports.CarryTurn(entity);
			}
			// The creature that comes out of the far stone moves on its map: there it looks at the mountains and the water,
			// and far about for the towns and the temples
			if (carried == Carry::Arrived)
			{
				LookAtLand(entity, creature, mind, transform);
				LookFarAbout(entity, mind, transform);
			}
			if (carried == Carry::Done)
			{
				creature_mind::CarryDone(mind.idle);
			}
		}
		if (commands.stopTeleportControl && Locator::teleportSystem::has_value())
		{
			Locator::teleportSystem::value().StopControl(entity);
		}
		// A building mended by a fifth of its life, and the tree used up mending it gone. When the building gets all of
		// its life again, the end of the plan can decrease its desire.
		if (commands.repair.has_value() && registry.Valid(static_cast<entt::entity>(*commands.repair)))
		{
			const auto repaired = static_cast<entt::entity>(*commands.repair);
			static_cast<void>(ecs::world_objects::IncreaseLife(repaired, creature_mind::k_RepairLife));
			if (creature_mind::RepairedToFullLife(ecs::world_objects::LifeOf(repaired)))
			{
				mind.desireSeenTo = true;
			}
		}
		if (commands.seesToDesire)
		{
			mind.desireSeenTo = true;
		}
		if (commands.heal.has_value())
		{
			Heal(entity, static_cast<entt::entity>(*commands.heal));
		}
		mind_town_help::Apply(registry, commands);
		if (commands.renderImmobile.has_value())
		{
			MakeStill(registry, static_cast<entt::entity>(*commands.renderImmobile));
		}
		if (commands.useUp.has_value() && registry.Valid(static_cast<entt::entity>(*commands.useUp)))
		{
			ecs::world_objects::Remove(static_cast<entt::entity>(*commands.useUp));
		}
		// What its hand holds goes, and its hand is empty
		if (commands.useUpHeld && Locator::creatureObjectActionSystem::has_value())
		{
			if (const auto usedUpHeld = Locator::creatureObjectActionSystem::value().GetHeld(entity))
			{
				mind.idle.usedUpHeld = entt::to_integral(*usedUpHeld);
				ecs::world_objects::Remove(*usedUpHeld);
			}
		}
		if (commands.restSitting && Locator::creaturePhysiologySystem::has_value())
		{
			Locator::creaturePhysiologySystem::value().SitTurn(entity);
		}
		// Its own fire stops: it cools to the temperature of the air
		if (commands.coolDown && Locator::fireSystem::has_value())
		{
			Locator::fireSystem::value().PutOut(entity);
		}
		if (commands.healItself)
		{
			HealItself(registry, entity);
		}
		// It faints, and those near it react as to a death. The reaction belongs to no player.
		if (commands.fainted && Locator::reactionSystem::has_value())
		{
			Locator::reactionSystem::value().Create(
			    {.initiator = entity, .type = Reaction::ReactToDeath, .position = transform.position, .playerless = true});
		}
		if (commands.effect == creature_mind::Effect::RanAway)
		{
			EndRunAway(entity, mind, commands.effectObject);
		}
		if (commands.douse.has_value())
		{
			if (Douse(entity, static_cast<entt::entity>(*commands.douse)))
			{
				mind.desireSeenTo = true;
			}
		}
		TakeEffect(entity, commands, *mind.desires, mind.planActive);
		// After this step, the plan acts on the creature, not on the thing that it chose
		if (commands.actOnItself && mind.planner.current.has_value())
		{
			mind.planner.current->object = entt::to_integral(entity);
		}
		// Once it has picked up the food it took from a pile, the pile gives that much up
		if (commands.effect == creature_mind::Effect::TookFromPile)
		{
			GiveUpTakenFood(registry, mind);
		}
		if (commands.effect == creature_mind::Effect::Observed && commands.effectObject.has_value())
		{
			const auto thing = static_cast<entt::entity>(*commands.effectObject);
			if (const auto belief = BeliefOf(registry, thing, entity))
			{
				mind.observedKinds.insert(belief->type);
			}
			else if (registry.AllOf<Animal>(thing))
			{
				// Animals are their own kind of thing to a creature, though nothing else it knows of them is kept yet
				mind.observedKinds.insert(creature_tree::belief_types::k_Animal);
			}
		}
		// On each turn from the blow of a stomp to the end of its animation, a living thing on the map under the foot
		// takes the crush. When the stomp is the last step and the thing has no life, the kill counts and the desire of
		// the action is seen to, on each of these turns.
		if (commands.stomp.has_value() && Locator::creatureObjectActionSystem::has_value())
		{
			const auto thing = static_cast<entt::entity>(commands.stomp->object);
			if (Locator::creatureObjectActionSystem::value().CrushUnderFoot(entity, thing) && commands.stomp->lastStep)
			{
				StompedToDeath(entity, mind, thing);
			}
		}
		// Having looked another creature over, it notes when, and waits a while before it does again
		if (commands.effect == creature_mind::Effect::Inspected && commands.effectObject.has_value())
		{
			if (auto* inspected = mind.beliefs.Find(static_cast<entt::entity>(*commands.effectObject));
			    inspected != nullptr && Locator::reactionSystem::has_value())
			{
				inspected->inspected = Locator::reactionSystem::value().CurrentTurn();
			}
		}
		CountCompleted(entity, mind, commands, GetTables());
		if (commands.object.has_value() && mind.planActive && mind.planner.current.has_value())
		{
			creature_lost_thing::StopUseBeforeThrowIntoStore(*mind.planner.current, *commands.object,
			                                                 entt::to_integral(entity));
		}
		// Taking in what a thing is does nothing for the desire the plan serves
		const bool plannedMeal = commands.effect == creature_mind::Effect::Eat && mind.planActive;
		if (commands.effect != creature_mind::Effect::None && commands.effect != creature_mind::Effect::Observed &&
		    commands.effect != creature_mind::Effect::Inspected && commands.effect != creature_mind::Effect::TookFromPile &&
		    commands.effect != creature_mind::Effect::TookFromHand && commands.effect != creature_mind::Effect::Stomped &&
		    !plannedMeal)
		{
			mind.satisfiedByEffect = true;
		}
		FollowAgenda(entity, mind);
		// The turns that the agenda runs steps for
		if (mind.agendaTurnsSerial != mind.idle.serial)
		{
			mind.agendaTurnsSerial = mind.idle.serial;
			mind.agendaTurns = 0;
		}
		if (mind.idle.step < mind.idle.agenda.size())
		{
			++mind.agendaTurns;
		}
		// Once it has taken what the hand gave it, its desires choose what to do with it
		if (commands.effect == creature_mind::Effect::TookFromHand && commands.effectObject.has_value())
		{
			UseGift(entity, static_cast<entt::entity>(*commands.effectObject));
		}
		if (needs != nullptr)
		{
			needs->rest = creature_mind::IsUnconscious(mind.idle) ? CreatureNeeds::Rest::Unconscious
			              : creature_mind::IsAsleep(mind.idle)    ? CreatureNeeds::Rest::Asleep
			              : creature_mind::IsResting(mind.idle)   ? CreatureNeeds::Rest::Resting
			                                                      : CreatureNeeds::Rest::Awake;
		}

		// Looking about, the head turns to the most interesting thing in sight, or ahead
		mind.lookingAbout = commands.lookAbout;
		const auto focus = ScriptFocusPoint(registry, entity, mind);
		if (focus.has_value())
		{
			// A script's focus holds the head whatever the creature is doing
			animation.lookAt = focus;
		}
		else if (commands.gaze.has_value() && commands.gaze->runningAway)
		{
			// Running away, it looks back at what it runs from for a time, then looks about for a time
			const bool back =
			    creature_emote::RunLooksBack(mind.runLookBack, k_TurnsPerSecond, RunLookBackSeconds(creature.species),
			                                 RunLookAheadSeconds(creature.species));
			auto from = *commands.gaze;
			from.runningAway = false;
			const auto point = back ? GazePoint(registry, entity, from, moving) : std::nullopt;
			animation.lookAt = point.has_value() ? point : LookAbout(creature, transform, mind);
		}
		else if (commands.gaze.has_value() && commands.gaze->whileGoing)
		{
			// On the way to a place: the head looks at the place, at what it watches, or from side to side. Without
			// a point of its own, the look uses the top of the thing that it goes to, or else the place that the
			// creature walks to.
			const auto about = LookAbout(creature, transform, mind);
			auto place = commands.gaze->point;
			if (!place.has_value() && commands.gaze->object.has_value())
			{
				place = GazePoint(registry, entity, {.object = commands.gaze->object}, false);
			}
			if (const auto* walk = registry.TryGet<const CreatureLocomotion>(entity);
			    !place.has_value() && walk != nullptr && walk->destination.has_value())
			{
				place = glm::vec3(walk->destination->x, 0.0f, walk->destination->y);
			}
			animation.lookAt =
			    place.has_value() ? LookWhileGoing(entity, creature, transform, mind, *place).value_or(about) : about;
		}
		else
		{
			// A step with nothing to look at, or whose thing is gone, looks about. The pose of the body decides if
			// the head can turn to the point.
			const auto point = commands.gaze.has_value() ? GazePoint(registry, entity, *commands.gaze, moving) : std::nullopt;
			animation.lookAt = point.has_value() ? *point : LookAbout(creature, transform, mind);
		}
		// After the hold of a pick-up, the head and the eyes look at no point until the body has played the pick-up
		if (commands.noLookPoint)
		{
			animation.lookAt.reset();
		}
		if (eyes != nullptr)
		{
			eyes->lookAt = animation.lookAt;
		}
	};
	if (acts)
	{
		act();
	}
	// A joint action stops for both creatures when either stops what it does for it
	KeepJointAction(entity);
}

bool CreatureMindSystem::Replan(entt::entity creature, creature_mind::Activity activity,
                                std::vector<creature_mind::Step> agenda, bool stopsWalk, bool endsBody)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	auto* body = registry.TryGet<CreatureAnimation>(creature);
	if (mind == nullptr || body == nullptr || agenda.empty())
	{
		return false;
	}
	// It stops walking, unless a catch it has started is turning it to face what it catches: changing what it does
	// leaves a catch to its body
	// TODO(physics): whether the game's stopping cuts short the turn a catch starts with wasn't traced
	const bool catching =
	    Locator::creatureObjectActionSystem::has_value() && Locator::creatureObjectActionSystem::value().IsCatching(creature);
	if (Locator::creatureLocomotionSystem::has_value() && !catching && stopsWalk && _walkContinuesFor != creature)
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
	// What the hands do stops for the plan. A stop that ends the action of the body also ends a sitting or lying pose, a
	// dance and a point, which each play their end. An action that it is playing, such as reeling from a slap, plays
	// on: the new agenda waits for the body to be free. It lets go of any miracle it holds.
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
	{
		Locator::creatureObjectActionSystem::value().StopForPlan(creature, endsBody);
	}
	if (endsBody)
	{
		body->body = creature_layers::EndDance(creature_layers::EndLoop(body->body));
	}
	ReleaseCast(creature);
	RestEyes(registry, creature, registry.TryGet<CreatureEyes>(creature));
	Abandon(*mind);
	creature_mind::Plan(mind->idle, activity, std::move(agenda));
	// The area that holds the creature refused a walk of the new agenda: the creature stops and its plan fails
	if (std::exchange(mind->agendaRefused, false))
	{
		GiveUp(mind->idle);
	}
	return true;
}

bool CreatureMindSystem::ForceActivity(entt::entity creature, const ForcedActivity& forced)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	auto* body = mind != nullptr ? registry.TryGet<CreatureAnimation>(creature) : nullptr;
	const auto* tables = GetTables();
	if (mind == nullptr || body == nullptr || tables == nullptr || !mind->learnt.has_value() || !mind->desires.has_value())
	{
		return false;
	}
	const auto action = creature_mind_tables::FindAction(*tables, forced.action);
	if (!action.has_value())
	{
		return false;
	}
	if (forced.comeAcross && registry.Valid(forced.actionObject))
	{
		ComeAcross(creature, *mind, forced.actionObject);
	}
	// A forced plan first stops the reaction that the creature has
	EndReaction(creature);
	// The current action stops as a failure. This occurs also when the creature finds no new plan. It also stops a fight
	// that the creature is in, out of its duel.
	EndFightForPlan(creature, forced.fightBodyEndsAgain);
	// An action that the body started plays on, and a point plays its end
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
	{
		Locator::creatureObjectActionSystem::value().StopForPlan(creature, true);
	}
	Abandon(*mind);
	ReleaseCast(creature);
	const auto toNumber = [](std::optional<entt::entity> entity) {
		return entity.has_value() ? std::optional(entt::to_integral(*entity)) : std::nullopt;
	};
	const auto actionObject = forced.actionObject != entt::null ? toNumber(forced.actionObject) : std::nullopt;
	creature_plan_actions::Situation situation {
	    .instrument = toNumber(forced.instrument),
	    .handFull = ecs::creature_carried::CarriedBy(registry, creature).has_value(),
	    .activity = toNumber(forced.activityObject),
	};
	Situate(creature, situation);
	auto built = PlanAgenda(creature, *action, actionObject, situation);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} is made to {} on {}: {}", entt::to_integral(creature), forced.action,
	                    actionObject.value_or(0), built.has_value() ? "planned" : "can't be planned");
	if (!built.has_value())
	{
		// The plan couldn't be made: it is left doing nothing, to decide afresh, once a pose it was in has ended and an
		// action it was playing has played out
		if (Locator::creatureLocomotionSystem::has_value())
		{
			Locator::creatureLocomotionSystem::value().Stop(creature);
		}
		body->body = creature_layers::EndDance(creature_layers::EndLoop(body->body));
		creature_mind::Plan(mind->idle, creature_mind::Activity::None, {});
		return false;
	}
	auto& [executor, agenda] = *built;
	if (!Replan(creature, executor->activity, std::move(agenda)))
	{
		return false;
	}
	const auto priority = forced.unforcedPriority.value_or(creature_planner::k_ForcedScore);
	mind->planner.current = creature_planner::Plan {
	    .desire = forced.desire,
	    .action = *action,
	    .object = actionObject,
	    .goalUsefulness = priority,
	    .actionPriority = priority,
	    .priority = priority,
	    .activityObject = toNumber(forced.activityObject),
	    .instrument = situation.instrument,
	};
	mind->planActive = true;
	mind->planForced = forced.forced && !forced.unforcedPriority.has_value();
	mind->planSerial = mind->idle.serial;
	mind->agendaSeen = mind->idle.serial;
	mind->satisfiedByEffect = false;
	mind->desireSeenTo = false;
	// It is remembered for the player's feedback, unless nothing says why it wants what the plan serves
	auto& desire = (*mind->desires)[forced.desire];
	if (const auto source = creature_learning::PlanSource(desire); source.has_value())
	{
		const auto& info = tables->actions[*action];
		RememberAction(creature, *mind,
		               {
		                   .action = *action,
		                   .desire = forced.desire,
		                   .object = actionObject,
		                   .belief = actionObject.has_value()
		                                 ? ActedOnBeliefOf(registry, static_cast<entt::entity>(*actionObject), creature)
		                                 : std::nullopt,
		                   .activity = toNumber(forced.activityObject),
		                   .activityBelief = forced.activityObject.has_value()
		                                         ? BeliefOf(registry, *forced.activityObject, creature)
		                                         : std::nullopt,
		                   .learnable = info.learnable,
		                   .windowSeconds = info.learningWindowSeconds,
		                   .source = *source,
		                   .ordered = forced.ordered,
		               });
	}
	creature_learning::SuppressOpposed(*mind->desires, forced.desire, tables->dependencies, k_TurnsPerSecond);
	return true;
}

void CreatureMindSystem::AbandonAction(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr)
	{
		Abandon(*mind);
		// The action stops as a failure, and its agenda ends with it. The creature then chooses a new plan.
		mind->idle.step = mind->idle.agenda.size();
		mind->idle.stepStarted = false;
	}
	ReleaseCast(creature);
}

void CreatureMindSystem::GiveUpAction(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	// The failure stops a fight that the creature is in, for both creatures, with the strength gains of the stop
	if (registry.Valid(creature) && registry.AllOf<CreatureFighting>(creature) && Locator::creatureFightSystem::has_value())
	{
		Locator::creatureFightSystem::value().CallOffForPlan(creature);
	}
	AbandonAction(creature);
}

void CreatureMindSystem::FailAction(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	EndJointAction(creature);
	AbandonAction(creature);
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
	// The loop that the body plays ends, as for a change of plan
	if (auto* body = registry.TryGet<CreatureAnimation>(creature))
	{
		body->body = creature_layers::EndLoop(body->body);
	}
}

void CreatureMindSystem::SetAgendaPriority(entt::entity creature, float priority)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	// It stands only until the next plan is carried out, which brings its own
	if (mind != nullptr && mind->planner.current.has_value())
	{
		mind->planner.current->priority = priority;
	}
}
