/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureFightSystem.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <chrono>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/mat3x3.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFizz.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureMode.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreatureRestAndHome.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureWalkPlanner.h"
#include "Creature/FightMarks.h"
#include "CreatureFightSystemDetail.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureArena.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreaturePlacement.h"
#include "ECS/CreatureScars.h"
#include "ECS/Map.h"
#include "ECS/PosedModel.h"
#include "ECS/Registry.h"
#include "ECS/ShownCreatureFaces.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/FightTranscriptNote.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/ThingCircles.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/ThingModel.h"
#include "ECS/WorldObjects.h"
#include "Input/LocalPlayer.h"
#include "Locator.h"
#include "Particles/ParticleDrawFrame.h"
#include "Physics/TurnRules.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace fight = openblack::creature_fight;
namespace feedback = openblack::creature_feedback;
using Pace = CreatureLocomotionSystemInterface::Pace;
using namespace openblack::ecs::systems::fight_detail;

namespace
{
constexpr float k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();

constexpr auto k_TurnMilliseconds =
    static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(TimeSystemInterface::k_TurnDuration).count());
} // namespace

namespace openblack::ecs::systems::fight_detail
{
int32_t TurnMsOf(const CreatureAnimation* animation)
{
	const float pace = animation != nullptr ? animation->playbackScale : 1.0f;
	return creature_walk_planner::TurnMs(pace, static_cast<int32_t>(k_TurnMilliseconds));
}

float HeightOf(float size)
{
	return feedback::k_HeightAtSizeOne * std::max(size, 0.05f);
}

glm::vec2 Flat(const glm::vec3& point)
{
	return {point.x, point.z};
}

float GroundAt(glm::vec2 point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

float DurationOf(entt::entity creature, size_t animation)
{
	if (!Locator::creatureAnimationSystem::has_value())
	{
		return 0.0f;
	}
	return Locator::creatureAnimationSystem::value().AnimationDuration(creature, animation).value_or(0.0f);
}

bool Has(entt::entity creature, size_t animation)
{
	return DurationOf(creature, animation) > 0.0f;
}

glm::vec2 WorldToLocal(glm::vec2 world, GameHeading heading)
{
	const auto ahead = creature_locomotion::DirectionOf(heading);
	const glm::vec2 right {-ahead.y, ahead.x};
	return {glm::dot(world, right), glm::dot(world, ahead)};
}

void Shift(CreatureLocomotion& locomotion, glm::vec2 by)
{
	for (auto* point : {&locomotion.fromPosition, &locomotion.toPosition})
	{
		point->x += by.x;
		point->z += by.y;
		point->y = GroundAt(Flat(*point));
	}
}

std::optional<std::pair<const graphics::L3DMesh*, glm::mat4>> DrawnMeshOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* mesh = registry.Valid(creature) ? registry.TryGet<const Mesh>(creature) : nullptr;
	const auto* transform = registry.Valid(creature) ? registry.TryGet<const Transform>(creature) : nullptr;
	if (mesh == nullptr || transform == nullptr || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return std::nullopt;
	}
	return std::pair {&*meshes.Handle(mesh->id),
	                  creature::PlacementMatrix(transform->position, transform->rotation, transform->scale)};
}

PlayerNames OwnPlayer()
{
	return input::LocalPlayer(Locator::handSystem::has_value() ? &Locator::handSystem::value() : nullptr);
}

bool IsHumanGod(PlayerNames player)
{
	if (Locator::playerSystem::has_value() && Locator::playerSystem::value().IsComputerPlayer(player))
	{
		return false;
	}
	if (!Locator::handSystem::has_value())
	{
		return player == PlayerNames::PLAYER_ONE;
	}
	return Locator::handSystem::value().GetHandsOf(player).has_value();
}

std::optional<entt::entity> PrimaryCreatureOf(const ecs::Registry& registry, PlayerNames player)
{
	std::optional<entt::entity> first;
	registry.Each<const Creature>([&first, player](entt::entity entity, const Creature& creature) {
		if (creature.owner == player && (!first.has_value() || entt::to_entity(entity) < entt::to_entity(*first)))
		{
			first = entity;
		}
	});
	return first;
}

bool InDuel(const ecs::Registry& registry, entt::entity creature)
{
	const auto* fighting = registry.TryGet<const CreatureFighting>(creature);
	return fighting != nullptr && fighting->stage == CreatureFighting::Stage::Duel;
}

bool FightsOn(const ecs::Registry& registry, entt::entity creature, const CreatureFighting& fighting)
{
	if (!registry.Valid(fighting.opponent))
	{
		return false;
	}
	if (InDuel(registry, fighting.opponent))
	{
		return true;
	}
	const auto* other = registry.TryGet<const CreatureFighting>(fighting.opponent);
	return fighting.partnerLeft != CreatureFighting::PartnerLeft::None && other != nullptr && other->opponent == creature;
}

bool OpponentStartsDuel(const ecs::Registry& registry, entt::entity creature, const CreatureFighting& fighting)
{
	const auto* other =
	    registry.Valid(fighting.opponent) ? registry.TryGet<const CreatureFighting>(fighting.opponent) : nullptr;
	return other != nullptr && other->opponent == creature && other->stage == CreatureFighting::Stage::Square && other->played;
}

float ForwardExtentOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* locomotion = registry.TryGet<const CreatureLocomotion>(creature);
	if (animation == nullptr || transform == nullptr || locomotion == nullptr)
	{
		return 0.0f;
	}
	if (registry.AllOf<CreatureFighting>(creature) && !animation->lastTurnBones.empty())
	{
		std::vector<glm::vec2> lastTurn(animation->lastTurnBones.size());
		std::ranges::transform(animation->lastTurnBones, lastTurn.begin(), [](const glm::vec3& bone) { return Flat(bone); });
		return fight::ForwardExtentFromLastTurn(lastTurn, Flat(locomotion->toPosition), locomotion->heading.Radians());
	}
	const auto pose = creature::TurnPose(animation->newerTurnPose, animation->boneMatrices,
	                                     creature::PlacementMatrix(transform->position, transform->rotation, transform->scale));
	std::vector<glm::vec2> bones;
	bones.reserve(pose.bones.size());
	for (const auto& bone : pose.bones)
	{
		bones.push_back(Flat(glm::vec3(pose.placement * bone[3])) - Flat(transform->position));
	}
	return fight::ForwardExtent(bones, creature_locomotion::DirectionOf(locomotion->heading));
}
} // namespace openblack::ecs::systems::fight_detail

namespace
{
/// A face is due again once its wait is within this much of over
constexpr float k_FaceDue = 1e-3f;

/// The stages before the duel give up waiting after this long
constexpr float k_ApproachSeconds = 30.0f;

/// How many times a creature completed the fight action
uint32_t FightsCompleted(entt::entity creature)
{
	if (!Locator::creatureMindSystem::has_value())
	{
		return 0;
	}
	auto& minds = Locator::creatureMindSystem::value();
	const auto* tables = minds.GetTables();
	const auto action = tables != nullptr ? creature_mind_tables::FindAction(*tables, "Fight") : std::nullopt;
	return action.has_value() ? minds.GetActionCount(creature, *action) : 0;
}

float LifeOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* needs = registry.TryGet<const CreatureNeeds>(creature);
	return needs != nullptr ? needs->needs.life : 1.0f;
}

void PlaceAt(CreatureLocomotion& locomotion, Transform& transform, glm::vec3 at)
{
	at.y = GroundAt(Flat(at));
	locomotion.fromPosition = at;
	locomotion.toPosition = at;
	transform.position = at;
}

/// Turns a creature towards a heading by at most an angle, at once
void TurnTowards(CreatureLocomotion& locomotion, GameHeading heading, float maxRadians)
{
	const auto difference = heading.TurnFrom(locomotion.heading);
	const auto turned = GameHeading(locomotion.heading.Radians() + std::clamp(difference, -maxRadians, maxRadians));
	locomotion.heading = turned;
	locomotion.targetHeading = turned;
	locomotion.fromHeading = turned;
	locomotion.toHeading = turned;
}

void PlayOnce(CreatureAnimation& animation, size_t played, bool mirrored = false)
{
	animation.body = {};
	if (const auto once = creature_layers::PlayOnce(animation.body, played, mirrored))
	{
		animation.body = *once;
	}
}

void SetEyes(ecs::Registry& registry, entt::entity creature, creature_eyes::Mode mode)
{
	if (auto* eyes = registry.TryGet<CreatureEyes>(creature))
	{
		eyes->mode = mode;
	}
}

/// Sets a creature fizzing out of sight or back in
void SetFizz(entt::entity creature, float target, float seconds)
{
	if (Locator::creatureFizzSystem::has_value())
	{
		Locator::creatureFizzSystem::value().SetFizz(creature, target, seconds, false);
	}
}

/// Carried home and stopped part way, a creature is back in full sight at once
void StopCarryingHome(entt::entity creature, const CreatureKnockedOut& knockedOut)
{
	if (knockedOut.stage == CreatureKnockedOut::Stage::FadingOut || knockedOut.stage == CreatureKnockedOut::Stage::FadingIn)
	{
		SetFizz(creature, 0.0f, 0.0f);
	}
}

/// A fighter turns at once to face its opponent straight on
void FaceOpponentNow(ecs::Registry& registry, entt::entity creature, entt::entity opponent)
{
	auto* locomotion = registry.TryGet<CreatureLocomotion>(creature);
	if (locomotion == nullptr || !registry.Valid(opponent) || !registry.AllOf<Transform>(opponent))
	{
		return;
	}
	const auto towards = Flat(registry.Get<const Transform>(opponent).position) - Flat(locomotion->toPosition);
	if (glm::length(towards) > 0.0f)
	{
		TurnTowards(*locomotion, creature_locomotion::HeadingOf(towards), glm::pi<float>());
	}
}

/// Whether the body of the creature is still in its duel: the fight of the creature has not ended yet
bool BodyInDuel(const ecs::Registry& registry, entt::entity creature)
{
	const auto* fighting = registry.TryGet<const CreatureFighting>(creature);
	return fighting != nullptr && fighting->stage == CreatureFighting::Stage::Duel && !fighting->ended;
}

/// Whether nothing can bring the life of the creature to nearly none: a script holds it in a cutscene, or it is
/// indestructible
bool LifeProtected(const ecs::Registry& registry, entt::entity creature)
{
	return ecs::world_objects::HeldDuringCutscene(creature) || registry.AllOf<Indestructible>(creature);
}

/// How many times the life of the creature pays for its fight when its body ends its fight a number of times. Its life
/// pays each time while its body is in a state of the fight, else never.
uint32_t LifePaymentsOf(const ecs::Registry& registry, entt::entity creature, uint32_t bodyEnds)
{
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	return fighting != nullptr && fight::LifePaysAtStop(fighting->fighter.state) ? bodyEnds : 0U;
}

/// Whether a stage comes after the duel and ends with no win plan: the response, or the stop with no response
constexpr bool IsAfterDuel(CreatureFighting::Stage stage)
{
	return stage == CreatureFighting::Stage::Respond || stage == CreatureFighting::Stage::Stop;
}

/// The partner of the creature, if the partner is after its duel against the creature and has no win plan
std::optional<entt::entity> PartnerAfterDuel(const ecs::Registry& registry, entt::entity creature,
                                             const CreatureFighting& fighting)
{
	const auto* other =
	    registry.Valid(fighting.opponent) ? registry.TryGet<const CreatureFighting>(fighting.opponent) : nullptr;
	if (other == nullptr || other->opponent != creature || !IsAfterDuel(other->stage))
	{
		return std::nullopt;
	}
	return fighting.opponent;
}

/// The plan of the creature stopped, so the plan of its partner stops as a failure too. The creature that started the
/// fight gives up its plan. The other creature gives up its plan to obey.
void PartnerPlanFails(entt::entity partner, entt::entity creature, bool partnerStarted)
{
	if (!Locator::creatureMindSystem::has_value())
	{
		return;
	}
	auto& minds = Locator::creatureMindSystem::value();
	if (partnerStarted)
	{
		minds.AbandonAction(partner);
	}
	else
	{
		minds.ObeyEnded(partner, creature);
	}
}

/// The result of the fight for a creature that its partner stopped: a creature that had no life left lost
std::optional<bool> ResultWhenStopped(CreatureFighting::PartnerLeft why)
{
	return why == CreatureFighting::PartnerLeft::Won ? std::optional(false) : std::nullopt;
}

/// When the bodies of two fighters overlap, both move apart by half the overlap, along the direction that the first one
/// faces. Nothing moves when the last action that the opponent started is a blow.
void PullApartFighters(ecs::Registry& registry, entt::entity creature, const CreatureFighting& fighting)
{
	const auto opponent = fighting.opponent;
	const auto* other = registry.TryGet<const CreatureFighting>(opponent);
	auto* self = registry.TryGet<CreatureLocomotion>(creature);
	auto* otherBody = registry.TryGet<CreatureLocomotion>(opponent);
	if (other == nullptr || self == nullptr || otherBody == nullptr || fight::IsAttack(other->fighter.lastAction))
	{
		return;
	}
	const auto at = Flat(self->toPosition);
	const auto otherAt = Flat(otherBody->toPosition);
	const auto extents = ForwardExtentOf(registry, creature) + ForwardExtentOf(registry, opponent);
	const auto pulled =
	    fight::PullApartOf(fighting.arena, at, otherAt, creature_locomotion::DirectionOf(self->heading), extents);
	if (pulled.first.has_value())
	{
		Shift(*self, *pulled.first - at);
	}
	if (pulled.second.has_value())
	{
		Shift(*otherBody, *pulled.second - otherAt);
	}
	if (pulled.first.has_value() || pulled.second.has_value())
	{
		registry.SetDirty();
		fight_transcript::Note(creature, "pull_apart", [&] {
			return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
			                                 {"apart", glm::distance(at, otherAt)},
			                                 {"self_from_middle", glm::distance(at, fighting.arena.centre)},
			                                 {"opponent_from_middle", glm::distance(otherAt, fighting.arena.centre)},
			                                 {"extents", extents},
			                                 {"moved_self", pulled.first.has_value()},
			                                 {"moved_opponent", pulled.second.has_value()}};
		});
	}
}

/// The heading of the creature, as its legs keep it, or from its turn in the world before its legs start
GameHeading HeadingOf(const ecs::Registry& registry, entt::entity creature)
{
	if (const auto* moving = registry.TryGet<const CreatureLocomotion>(creature); moving != nullptr && moving->started)
	{
		return moving->heading;
	}
	const auto ahead = -(registry.Get<const Transform>(creature).rotation * glm::vec3(0.0f, 0.0f, 1.0f));
	return GameHeading::Of(ahead.x, ahead.z);
}

/// Whether the body of the creature is busy: it moves or turns, it plays an action, or it lets go of a thing or catches a
/// thing
bool BodyBusy(const ecs::Registry& registry, entt::entity creature, const CreatureLocomotionSystemInterface* locomotion)
{
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const bool lettingGo =
	    Locator::creatureObjectActionSystem::has_value() && Locator::creatureObjectActionSystem::value().PlaysOnAlone(creature);
	return (locomotion != nullptr && locomotion->IsMoving(creature)) || lettingGo ||
	       (animation != nullptr && creature_layers::IsPlaying(animation->body));
}

/// Whether the creature can start its duel on this turn. Its body refuses while it is busy, and a body with no clips for
/// the start, the stance and the finish of a fight refuses always. With no bodies loaded, nothing plays and the duel
/// starts. A creature that the hand of another creature holds also refuses.
bool CanStartDuel(const ecs::Registry& registry, entt::entity creature, const CreatureLocomotionSystemInterface* locomotion)
{
	if (BodyBusy(registry, creature, locomotion))
	{
		return false;
	}
	using namespace fight::animations;
	const bool clips = !Locator::creatureAnimationSystem::has_value() ||
	                   (Has(creature, k_Start) && Has(creature, k_Stance) && Has(creature, k_Finish));
	return clips && !registry.AllOf<HeldByCreature>(creature);
}

/// Does one game turn of the creature's turn to face its opponent in the face-off or in the arena. It turns from its map
/// position to the map position of the opponent where the opponent is now.
void FaceOpponentTurn(const ecs::Registry& registry, entt::entity creature, CreatureFighting& fighting,
                      CreatureLocomotionSystemInterface* locomotion)
{
	using Phase = creature_turn_to_face::Turn::Phase;
	if (fighting.faceTurn.phase == Phase::Idle || fighting.faceTurn.phase == Phase::Done)
	{
		return;
	}
	// With no legs, nothing turns
	if (locomotion == nullptr)
	{
		fighting.faceTurn.phase = Phase::Done;
		return;
	}
	const auto here = Flat(registry.Get<const Transform>(creature).position);
	const auto there = Flat(registry.Get<const Transform>(fighting.opponent).position);
	const creature_turn_to_face::Senses senses {
	    .onTop = creature_cast_moves::MapPointDistance(there, here) < creature_cast_moves::k_OnTopDistance,
	    .facing = creature_cast_moves::Facing(here, HeadingOf(registry, creature), there),
	    .bodyBusy = BodyBusy(registry, creature, locomotion),
	};
	const auto hold = creature_turn_to_face::HoldTurns(fight::k_FaceOffHoldSeconds, 1000 / k_TurnMilliseconds);
	creature_turn_to_face::Step(fighting.faceTurn, senses, hold, [&] {
		return locomotion->TurnToHeading(creature, creature_cast_moves::HeadingTo(here, there));
	});
}

/// The creature stops what it does, as when its plan stops or when the first creature of its fight gives it an order. A
/// walk slows to a stand, and a loop of the body continues to its end. An action that the body plays continues. The plan
/// of its hands stops and also stops the body: each action that the body started plays on to its end, and a point plays
/// its end. It keeps what it holds.
void StopWhatItDoes(ecs::Registry& registry, entt::entity creature)
{
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
	if (Locator::creatureObjectActionSystem::has_value())
	{
		constexpr bool k_StopsBody = true;
		Locator::creatureObjectActionSystem::value().StopForPlan(creature, k_StopsBody);
	}
	if (auto* animation = registry.TryGet<CreatureAnimation>(creature))
	{
		animation->body = creature_layers::EndLoop(animation->body);
	}
}
} // namespace

CreatureFightSystemInterface::StartResult CreatureFightSystem::StartFight(entt::entity creature, entt::entity opponent)
{
	return Start(creature, opponent, true);
}

CreatureFightSystemInterface::StartResult CreatureFightSystem::Start(entt::entity creature, entt::entity opponent, bool ask)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto ready = [&registry](entt::entity entity) {
		return registry.Valid(entity) && registry.AllOf<Creature, Transform, CreatureAnimation, CreatureLocomotion>(entity);
	};
	if (creature == opponent || !ready(creature) || !ready(opponent))
	{
		return StartResult::NoOpponent;
	}
	// A fight that stopped, and that the hand does not hold, is only a move that finishes. The question to the other
	// creature does not wait for it.
	const auto finishesMove = [&registry](entt::entity entity) {
		const auto* fighting = registry.TryGet<const CreatureFighting>(entity);
		return fighting != nullptr && fighting->stage == CreatureFighting::Stage::Stop && !fighting->held;
	};
	const auto busy = [&registry, &finishesMove](entt::entity entity) {
		return registry.AllOf<CreatureKnockedOut>(entity) ||
		       (registry.AllOf<CreatureFighting>(entity) && !finishesMove(entity));
	};
	if (busy(creature) || busy(opponent))
	{
		return StartResult::Busy;
	}
	if (!fight::HealthyEnoughToFight(LifeOf(registry, creature)))
	{
		return StartResult::TooWeak;
	}
	// Every fight starts by asking the other creature, which may refuse. The other creature is asked one time only: a
	// fight that waits for a move to finish keeps the answer.
	if (ask)
	{
		const bool dead =
		    Locator::creatureMindSystem::has_value() && Locator::creatureMindSystem::value().IsLyingDead(opponent);
		if (!fight::AgreesToFight(HeightOf(ShownSize(registry.Get<const Creature>(opponent))),
		                          HeightOf(ShownSize(registry.Get<const Creature>(creature))), dead,
		                          registry.AllOf<ScriptControlled>(opponent), registry.AllOf<ScriptControlled>(creature)))
		{
			return StartResult::Refused;
		}
		// A creature that does a joint action with a third creature refuses too
		if (const auto* link = registry.TryGet<const CreaturePartner>(opponent); link != nullptr && link->partner != creature)
		{
			return StartResult::Refused;
		}
	}
	// The other creature agreed. The face-off starts when both moves have finished.
	if (finishesMove(creature) || finishesMove(opponent))
	{
		return StartResult::Finishing;
	}
	// The arena appears only after the face-off, so a fight with no room for an arena starts and then stops
	for (const auto& [self, other] : {std::pair(creature, opponent), std::pair(opponent, creature)})
	{
		const auto& body = registry.Get<const Creature>(self);
		// The first creature stops what it does, as its plan stops. The other creature continues with what it did
		// until the first creature gives it its first order. The turn to face waits for an action that the body plays.
		// Each creature lets go of what it holds before its taunt.
		if (self == creature)
		{
			StopWhatItDoes(registry, self);
		}

		auto& record = registry.AllOf<CreatureFightRecord>(self) ? registry.Get<CreatureFightRecord>(self)
		                                                         : registry.Assign<CreatureFightRecord>(self);
		const auto* needs = registry.TryGet<const CreatureNeeds>(self);
		const auto bodyNeeds = needs != nullptr ? needs->needs : creature_physiology::Needs {};

		CreatureFighting fighting {
		    .stage = CreatureFighting::Stage::FaceOff,
		    .part = self == creature ? CreatureFighting::Part::Obeys : CreatureFighting::Part::Asked,
		    .opponent = other,
		    .madeArena = self == creature,
		};
		auto& fighter = fighting.fighter;
		fighter.health = fight::FightHealthAtStart(bodyNeeds.life);
		fighter.stamina = fight::StaminaAtStart(bodyNeeds.energy, bodyNeeds.exhaustion);
		// A human god directs their own creature, in each slot of a network game. The computer fights any other.
		fighter.control = IsHumanGod(body.owner) ? fight::Control::Player : fight::Control::Computer;
		fighter.computerWaitMs = fight::k_ComputerWaitsAtStartMs;
		// The creature that started the fight takes its learnt lean to choose its moves by; the other keeps the lean
		// of the last fight that it started
		if (self == creature)
		{
			record.tendency = fight::TendencyAtStart(FightsCompleted(self), record.tendency, body.alignment);
			record.choosingTendency = record.tendency;
		}
		fighter.tendency = record.choosingTendency;
		registry.AssignOrReplace<CreatureFighting>(self, std::move(fighting));
	}
	// The taunt of each creature is chosen now from the game's random numbers, the other creature's first. One time in
	// two it is the rude gesture.
	if (Locator::gameRandom::has_value())
	{
		auto& game = Locator::gameRandom::value();
		registry.Get<CreatureFighting>(opponent).rude = game.GameRand(2) != 0;
		registry.Get<CreatureFighting>(creature).rude = game.GameRand(2) != 0;
	}
	const bool planned = _startReason == "plan" || _startReason == "anger";
	registry.Get<CreatureFighting>(creature).planned = planned;
	// The fight has the mind of the first creature. The other creature obeys from its first order.
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().FightStarted(creature, opponent, planned);
	}

	if (fight_transcript::Recording())
	{
		using fight_transcript::Id;
		// A fight that a script or a tool asks for is a script's fight when a script controls either creature
		const bool scripted = registry.AnyOf<ScriptControlled>(creature) || registry.AnyOf<ScriptControlled>(opponent);
		const auto reason = scripted && _startReason == "request" ? std::string_view("script") : _startReason;
		Locator::fightTranscript::value().Begin(creature, opponent,
		                                        {{"first", Id(creature)}, {"second", Id(opponent)}, {"reason", reason}});
		for (const auto self : {creature, opponent})
		{
			auto fields = Levels(registry, self);
			const auto& body = registry.Get<const Creature>(self);
			fields.emplace_back("species", static_cast<int32_t>(body.species));
			fields.emplace_back("owner", static_cast<int32_t>(body.owner));
			fields.emplace_back("script_controlled", registry.AllOf<ScriptControlled>(self));
			Locator::fightTranscript::value().Note(self, "fighter", std::move(fields), std::nullopt);
			_seen.erase(self);
			_actionCause.erase(self);
		}
	}
	_startReason = "request";

	return StartResult::Started;
}

void CreatureFightSystem::PrepareLand()
{
	if (Locator::entitiesRegistry::has_value())
	{
		[[maybe_unused]] const auto& avoid = LandAvoidOf(Locator::entitiesRegistry::value());
	}
}

void CreatureFightSystem::Reset()
{
	_pressed.reset();
	_markHeld = false;
	EndView("new_land");
	_lookSeconds = 0.0f;
	_seen.clear();
	_actionCause.clear();
	_flightPlace.reset();
	_screenCentre.reset();
	_fightExit = true;
	if (Locator::effectRequestSystem::has_value())
	{
		Locator::effectRequestSystem::value().SetFightMarks({});
	}
}

namespace
{
/// The other creature of a fight that its first creature sets up, while the other creature has no order yet. None when
/// the creature did not start the fight, or the other creature obeys already.
std::optional<entt::entity> AskedOnly(const ecs::Registry& registry, entt::entity creature)
{
	const auto* fighting = registry.TryGet<const CreatureFighting>(creature);
	if (fighting == nullptr || !fighting->madeArena || !IsLeadIn(fighting->stage) || !registry.Valid(fighting->opponent))
	{
		return std::nullopt;
	}
	const auto* other = registry.TryGet<const CreatureFighting>(fighting->opponent);
	if (other == nullptr || other->opponent != creature || other->part == CreatureFighting::Part::Obeys)
	{
		return std::nullopt;
	}
	return fighting->opponent;
}

/// The first creature stopped the fight before its first order: what the other creature does stops as a failure
void StopAsked(ecs::Registry& registry, std::optional<entt::entity> asked)
{
	if (!asked.has_value() || !registry.Valid(*asked))
	{
		return;
	}
	StopWhatItDoes(registry, *asked);
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().AbandonAction(*asked);
	}
}
} // namespace

void CreatureFightSystem::AbortFight(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.TryGet<CreatureFighting>(creature);
	if (fighting == nullptr)
	{
		return;
	}
	const auto opponent = fighting->opponent;
	const auto asked = AskedOnly(registry, creature);
	for (const auto self : {creature, opponent})
	{
		auto* side = registry.Valid(self) ? registry.TryGet<CreatureFighting>(self) : nullptr;
		if (side == nullptr)
		{
			continue;
		}
		const auto* other = registry.Valid(side->opponent) ? registry.TryGet<const CreatureFighting>(side->opponent) : nullptr;
		if (side->stage == CreatureFighting::Stage::Duel)
		{
			NoteFightOver(self, "aborted");
			EndFightFor(self, other == nullptr || side->fighter.health >= other->fighter.health);
			NoteStage(self, side->stage, CreatureFighting::Stage::Respond);
			side->stage = CreatureFighting::Stage::Respond;
			side->played = false;
			fight::Enter(side->fighter, fight::State::Finish);
		}
		else if (IsLeadIn(side->stage))
		{
			Leave(self);
		}
	}
	StopAsked(registry, asked);
}

void CreatureFightSystem::CallOff(entt::entity creature)
{
	CallOffWith(creature, fight::CallOffStart::Failure);
}

void CreatureFightSystem::CallOffForWalkBack(entt::entity creature)
{
	CallOffWith(creature, fight::CallOffStart::BodyFirst);
}

void CreatureFightSystem::CallOffForPlan(entt::entity creature)
{
	CallOffWith(creature, fight::CallOffStart::Failure, true);
}

void CreatureFightSystem::CallOffWith(entt::entity creature, fight::CallOffStart start, bool endedFightPays)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.TryGet<CreatureFighting>(creature);
	if (fighting == nullptr)
	{
		return;
	}
	const auto opponent = fighting->opponent;
	// The creature gives up its action as a failure. The stop makes each creature stop again through the partners, and
	// the body of a creature can end its fight more than one time. Each time, its life pays again.
	const auto bodyEnds = RunStops(creature, fight::StepsOf(start)).bodyEnds;
	const std::array payments {LifePaymentsOf(registry, creature, bodyEnds[0]),
	                           LifePaymentsOf(registry, opponent, bodyEnds[1])};
	const auto asked = AskedOnly(registry, creature);
	for (size_t index = 0; index < payments.size(); ++index)
	{
		const auto self = index == 0 ? creature : opponent;
		auto* side = registry.Valid(self) ? registry.TryGet<CreatureFighting>(self) : nullptr;
		if (side == nullptr)
		{
			continue;
		}
		// The fight is over, but a stop for a new plan ends the fight of the body again. The life pays again while the
		// body plays a move of the fight.
		if (endedFightPays && side->ended)
		{
			PayLifeForFight(self, payments.at(index), "stopped_again");
		}
		if (side->stage == CreatureFighting::Stage::Duel)
		{
			// Neither wins: each finishes what it is doing and stands, and its fight is over with nothing more to it. The
			// plan of the partner fails.
			if (self != creature && side->opponent == creature)
			{
				PartnerPlanFails(self, creature, side->madeArena);
			}
			NoteFightOver(self, "called_off");
			EndFightFor(self, std::nullopt, payments.at(index));
			NoteStage(self, side->stage, CreatureFighting::Stage::Respond);
			side->stage = CreatureFighting::Stage::Respond;
			side->played = true;
			side->fighter.stopping = true;
			side->fighter.queue.Clear();
			side->marks.Clear();
		}
		else if (IsLeadIn(side->stage))
		{
			// Both creatures are partners from the request. Thus the plan of the creature that started the fight fails
			// also before the duel. The departure of the other creature ends its plan.
			if (self != creature && side->opponent == creature && side->madeArena)
			{
				PartnerPlanFails(self, creature, true);
			}
			Leave(self);
		}
		else if (IsAfterDuel(side->stage) && (self == creature || side->opponent == creature))
		{
			// After the duel, the creature stops with no response. Its partner after the duel also stops, and the plan
			// of the partner fails.
			if (self == creature)
			{
				StopFight(self, std::nullopt, "called_off");
			}
			else
			{
				StopPartnerAfterDuel(self, creature);
			}
		}
	}
	StopAsked(registry, asked);
}

bool CreatureFightSystem::StartedFight(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	return fighting != nullptr && fighting->madeArena;
}

bool CreatureFightSystem::IsSettingUpFight(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	return fighting != nullptr && IsLeadIn(fighting->stage);
}

void CreatureFightSystem::Withdraw(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.TryGet<const CreatureFighting>(creature);
	if (fighting == nullptr)
	{
		return;
	}
	const auto opponent = fighting->opponent;
	const auto arena = fighting->arenaEntity;
	AbortFight(creature);
	// What it fought and where forget it: the opponent plays out the end of its fight alone. The opponent forgets it
	// first, so that its departure does not stop the opponent.
	if (auto* other = registry.Valid(opponent) ? registry.TryGet<CreatureFighting>(opponent) : nullptr;
	    other != nullptr && other->opponent == creature)
	{
		other->opponent = entt::null;
	}
	// It leaves at once, rather than finishing and standing, and lets go of an arena it made
	Leave(creature);
	if (auto* taken = registry.Valid(arena) ? registry.TryGet<CreatureArena>(arena) : nullptr; taken != nullptr)
	{
		taken->first = taken->first == creature ? entt::null : taken->first;
		taken->second = taken->second == creature ? entt::null : taken->second;
	}
	if (_view.has_value() && (_view->first == creature || _view->second == creature))
	{
		EndView("creature_withdrawn");
	}
}

bool CreatureFightSystem::IsFighting(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(creature) && registry.AllOf<CreatureFighting>(creature);
}

bool CreatureFightSystem::IsDuelling(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(creature) && InDuel(registry, creature);
}

void CreatureFightSystem::TakeForScript(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	const bool fought = registry.AllOf<CreatureFighting>(creature);
	if (fought)
	{
		CallOff(creature);
	}
	auto* knockedOut = registry.TryGet<CreatureKnockedOut>(creature);
	const bool roused = knockedOut != nullptr && !knockedOut->permanent;
	if (roused)
	{
		// It gets up where it is. If it was on its way home, it is back in full sight at once.
		StopCarryingHome(creature, *knockedOut);
		if (auto* animation = registry.TryGet<CreatureAnimation>(creature))
		{
			animation->body = creature_layers::EndLoop(animation->body);
		}
		SetEyes(registry, creature, creature_eyes::Mode::Calm);
		if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
		{
			needs->rest = CreatureNeeds::Rest::Awake;
		}
		registry.Remove<CreatureKnockedOut>(creature);
	}
	if (fought || roused)
	{
		fight_transcript::Note(creature, "taken_by_script", [&] {
			return fight_transcript::Fields {
			    {"creature", fight_transcript::Id(creature)}, {"called_off", fought}, {"got_up", roused}};
		});
	}
}

std::optional<entt::entity> CreatureFightSystem::OpponentOf(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	return fighting != nullptr ? std::optional(fighting->opponent) : std::nullopt;
}

void CreatureFightSystem::SetAutoFighting(entt::entity creature, bool autoFight)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature>(creature))
	{
		return;
	}
	// The computer fights it, or nobody does and it makes only the moves queued for it
	const auto control = autoFight ? fight::Control::Computer : fight::Control::None;
	if (auto* fighting = registry.TryGet<CreatureFighting>(creature))
	{
		fight_transcript::Note(creature, "control", [&] {
			return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
			                                 {"from", fight::Name(fighting->fighter.control)},
			                                 {"to", fight::Name(control)},
			                                 {"why", "script"}};
		});
		fighting->fighter.control = control;
	}
	auto& record = registry.AllOf<CreatureFightRecord>(creature) ? registry.Get<CreatureFightRecord>(creature)
	                                                             : registry.Assign<CreatureFightRecord>(creature);
	record.control = control;
}

bool CreatureFightSystem::IsAutoFighting(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return false;
	}
	// Anyone choosing its moves, the player or the computer
	if (const auto* fighting = registry.TryGet<const CreatureFighting>(creature))
	{
		return fighting->fighter.control != fight::Control::None;
	}
	const auto* record = registry.TryGet<const CreatureFightRecord>(creature);
	return record != nullptr && record->control != fight::Control::None;
}

fight::FightAction CreatureFightSystem::CurrentFightAction(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return fight::FightAction::Other;
	}
	if (const auto* fighting = registry.TryGet<const CreatureFighting>(creature))
	{
		return fight::FightActionOf(fighting->fighter.animation);
	}
	return registry.AllOf<CreatureKnockedOut>(creature) ? fight::FightAction::Fainted : fight::FightAction::Other;
}

uint32_t CreatureFightSystem::QueuedBlows(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	return fighting != nullptr ? fight::QueuedBlows(fighting->fighter.queue.Moves()) : 0;
}

float CreatureFightSystem::ForwardExtent(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(creature) ? ForwardExtentOf(registry, creature) : 0.0f;
}

void CreatureFightSystem::KnockOut(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, CreatureAnimation>(creature) ||
	    registry.AllOf<CreatureKnockedOut>(creature))
	{
		return;
	}
	// In a duel, its opponent wins it
	if (auto* fighting = registry.TryGet<CreatureFighting>(creature))
	{
		if (fighting->stage == CreatureFighting::Stage::Duel && InDuel(registry, fighting->opponent))
		{
			Win(fighting->opponent, creature, false);
			return;
		}
		Leave(creature);
	}
	Faint(creature);
}

void CreatureFightSystem::ForceFaint(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, CreatureAnimation>(creature) ||
	    registry.AllOf<CreatureKnockedOut>(creature))
	{
		return;
	}
	if (registry.AllOf<CreatureFighting>(creature))
	{
		Leave(creature);
	}
	Faint(creature);
}

void CreatureFightSystem::FaintForScript(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, CreatureAnimation>(creature) ||
	    registry.AllOf<CreatureKnockedOut>(creature))
	{
		return;
	}
	if (registry.AllOf<CreatureFighting>(creature))
	{
		Leave(creature);
	}
	Faint(creature, Leash::Keep);
}

void CreatureFightSystem::Faint(entt::entity creature, Leash leash)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().StopAtOnce(creature);
	}
	// The action that its body plays ends at two times the usual speed. What its hands do goes on, and it keeps what it
	// holds: before the faint starts, it lets go of the thing as before any action that needs empty hands.
	if (Locator::creatureAnimationSystem::has_value())
	{
		Locator::creatureAnimationSystem::value().Hurry(creature);
	}
	// The faint stops what its mind does as a failure, an emergency too. Until it gets up, it looks at no emergency.
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().AbandonAction(creature);
	}
	// Fainting drops the leash, but not when a script orders the creature dead
	if (leash == Leash::TakeOff && Locator::leashSystem::has_value() && Locator::leashSystem::value().IsLeashed(creature))
	{
		Locator::leashSystem::value().TakeOff(creature);
	}
	// Whether it will rest to get better once it has lain long enough is decided now, by how it is as it faints
	bool rest = false;
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
	{
		needs->rest = CreatureNeeds::Rest::Unconscious;
		rest = fight::NeedsRest(needs->needs.life, needs->needs.exhaustion);
	}
	// The spells on it are brought to their end
	if (auto* spells = registry.TryGet<CreatureSpells>(creature))
	{
		creature_spells::TryFinishAll(spells->spells);
	}
	// It is taken to its home: its temple's pen while the temple stands, else the home it was last given (where it was
	// made, unless a script moved it). Only a creature with no home at all stays where it lies.
	const auto here = registry.Get<const Transform>(creature).position;
	const auto home = creature_home::HomeOf(registry, creature).value_or(here);
	// The faint itself starts on the body on the next turn that the knock-out is looked after
	registry.AssignOrReplace<CreatureKnockedOut>(creature, CreatureKnockedOut {.rest = rest, .home = home});
	fight_transcript::Note(creature, "faint", [&] {
		auto fields = Levels(registry, creature);
		fields.emplace_back("rest_later", rest);
		fields.emplace_back("at_x", here.x);
		fields.emplace_back("at_z", here.z);
		fields.emplace_back("home_x", home.x);
		fields.emplace_back("home_z", home.z);
		fields.emplace_back("script_controlled", registry.AllOf<ScriptControlled>(creature));
		return fields;
	});
}

void CreatureFightSystem::StartFaint(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	// The faint also stops what the hands do, such as a catch or a let-go. The hand keeps what it holds.
	if (Locator::creatureObjectActionSystem::has_value())
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	// The faint also stops a walk at once. Between the knock-out and this turn, an action of the hands can start a new
	// walk to its thing. If the walk stays, the creature gets up to follow it and the faint does not end.
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().StopAtOnce(creature);
	}
	// The faint cuts what the body plays at once. The game's random numbers choose its side, one time in two the left
	// side.
	auto& animation = registry.Get<CreatureAnimation>(creature);
	const bool mirrored = Locator::gameRandom::has_value() && Locator::gameRandom::value().GameRand(2) == 1;
	animation.body = {
	    .kind = creature_layers::BodyAction::Kind::Sequence,
	    .phase = creature_layers::BodyAction::Phase::Start,
	    .animations = {fight::animations::k_Faint, fight::animations::k_Faint, fight::animations::k_GetUp},
	    .mirrored = mirrored,
	    .holdLoop = true,
	};
	shown_creature_faces::ChangeFace(registry, creature, animation.face,
	                                 [](const creature_layers::FaceLayer& face) { return creature_layers::RelaxFace(face); });
	SetEyes(registry, creature, creature_eyes::Mode::Closed);
	// The people and the creatures near it react to the faint as to a death. The reaction belongs to no player.
	if (Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::value().Create({.initiator = creature,
		                                         .type = Reaction::ReactToDeath,
		                                         .position = registry.Get<const Transform>(creature).position,
		                                         .playerless = true});
	}
	fight_transcript::Note(creature, "faint_starts", [&] {
		return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)}, {"mirrored", mirrored}};
	});
}

void CreatureFightSystem::KillPermanently(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	if (registry.AllOf<CreatureFighting>(creature))
	{
		AbortFight(creature);
		Leave(creature);
	}
	KnockOut(creature);
	if (auto* knockedOut = registry.TryGet<CreatureKnockedOut>(creature))
	{
		StopCarryingHome(creature, *knockedOut);
		knockedOut->permanent = true;
		knockedOut->stage = CreatureKnockedOut::Stage::Lying;
	}
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
	{
		needs->needs.life = 0.0f;
	}
}

void CreatureFightSystem::Resurrect(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* knockedOut = registry.Valid(creature) ? registry.TryGet<CreatureKnockedOut>(creature) : nullptr;
	if (knockedOut == nullptr)
	{
		return;
	}
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
	{
		needs->needs.life = fight::k_GetUpLife;
	}
	StopCarryingHome(creature, *knockedOut);
	knockedOut->permanent = false;
	knockedOut->fainting = false;
	knockedOut->handsChecked = false;
	knockedOut->stage = CreatureKnockedOut::Stage::GettingUp;
	knockedOut->seconds = 0.0f;
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.body = creature_layers::EndLoop(animation.body);
	SetEyes(registry, creature, creature_eyes::Mode::Calm);
}

bool CreatureFightSystem::IsKnockedOut(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(creature) && registry.AllOf<CreatureKnockedOut>(creature);
}

void CreatureFightSystem::ProcessTurn()
{
	ProcessStages();
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> fighters;
	registry.Each<CreatureFighting>([&fighters](entt::entity entity, CreatureFighting&) { fighters.push_back(entity); });
	for (const auto entity : fighters)
	{
		StepMoves(entity);
	}
	ProcessDrips();
	ProcessKnockedOut();
	NoteActions();
	for (const auto entity : fighters)
	{
		StaminaTurn(entity);
	}
}

void CreatureFightSystem::FightTurn(entt::entity creature)
{
	StageTurn(creature);
	KnockedOutTurn(creature);
}

CreatureFightSystemInterface::StartResult CreatureFightSystem::StartPlannedFight(entt::entity creature, entt::entity opponent,
                                                                                 bool agreed)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	const bool angry = mind != nullptr && mind->planner.current.has_value() &&
	                   mind->planner.current->desire == creature_desires::Desire::Anger;
	_startReason = angry ? "anger" : "plan";
	const auto result = Start(creature, opponent, !agreed);
	_startReason = "request";
	return result;
}

void CreatureFightSystem::ProcessStages()
{
	std::vector<entt::entity> fighters;
	Locator::entitiesRegistry::value().Each<CreatureFighting>(
	    [&fighters](entt::entity entity, CreatureFighting& /*fighting*/) { fighters.push_back(entity); });
	for (const auto entity : fighters)
	{
		StageTurn(entity);
	}
}

void CreatureFightSystem::StageTurn(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return;
	}
	auto* locomotion = Locator::creatureLocomotionSystem::has_value() ? &Locator::creatureLocomotionSystem::value() : nullptr;
	const auto moving = [locomotion](entt::entity who) { return locomotion != nullptr && locomotion->IsMoving(who); };

	auto* fighting = registry.TryGet<CreatureFighting>(entity);
	if (fighting == nullptr)
	{
		return;
	}
	// While the hand holds the creature, its fight steps wait. On the first turn of the hold, a creature in a duel leaves
	// the duel.
	if (Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().IsLocked(entity))
	{
		if (!fighting->held)
		{
			HandTookHold(entity);
		}
		return;
	}
	fighting->stageSeconds += k_TurnSeconds;
	// Squaring up to the opponent and fighting it, it shows its anger as it starts and every few seconds after. The
	// other creature starts when it obeys.
	if (fighting->stage <= CreatureFighting::Stage::Duel && fighting->part == CreatureFighting::Part::Obeys &&
	    Locator::creatureMindSystem::has_value())
	{
		fighting->faceSeconds -= k_TurnSeconds;
		if (fighting->faceSeconds < k_FaceDue)
		{
			Locator::creatureMindSystem::value().ShowFeeling(entity, creature_face::Cue::Anger);
			fighting->faceSeconds = creature_mind::k_FaceRepeatSeconds;
		}
	}
	const auto opponent = fighting->opponent;
	const bool opponentHere = registry.Valid(opponent) && registry.AllOf<Creature, Transform>(opponent);
	const auto& body = registry.Get<const Creature>(entity);
	auto& animation = registry.Get<CreatureAnimation>(entity);
	const auto next = [this, entity, fighting](CreatureFighting::Stage stage) {
		NoteStage(entity, fighting->stage, stage);
		fighting->stage = stage;
		fighting->stageSeconds = 0.0f;
		fighting->played = false;
	};

	// The creature that started the fight tells the other creature when to do each step before the duel
	const auto* partner = opponentHere ? registry.TryGet<const CreatureFighting>(opponent) : nullptr;
	const bool together = partner != nullptr && partner->opponent == entity;
	const bool starter = fighting->madeArena;
	// Each order stops what the other creature does, makes the other creature obey and gives it the plan to obey again.
	// The mind can change the components of the creature, so the fight of the creature is found again after it.
	const auto obey = [&registry](entt::entity who) -> CreatureFighting* {
		auto* side = registry.TryGet<CreatureFighting>(who);
		if (side == nullptr)
		{
			return nullptr;
		}
		StopWhatItDoes(registry, who);
		side->part = CreatureFighting::Part::Obeys;
		if (Locator::creatureMindSystem::has_value())
		{
			Locator::creatureMindSystem::value().FightOrdered(who, side->opponent);
		}
		return registry.TryGet<CreatureFighting>(who);
	};
	const auto order = [this, &obey](entt::entity who, CreatureFighting::Stage stage) {
		auto* side = obey(who);
		if (side == nullptr)
		{
			return;
		}
		NoteStage(who, side->stage, stage);
		side->stage = stage;
		side->stageSeconds = 0.0f;
		side->played = false;
	};
	if (IsLeadIn(fighting->stage) && !together)
	{
		Leave(entity);
		return;
	}
	// The other creature continues with what it did until its first order
	if (fighting->part != CreatureFighting::Part::Obeys)
	{
		return;
	}

	switch (fighting->stage)
	{
	case CreatureFighting::Stage::FaceOff:
	{
		using Phase = creature_turn_to_face::Turn::Phase;
		++fighting->faceOffTurns;
		// The creature that started the fight tells the mind of the other creature to stand still. Then it tells the
		// other creature to stop and turn to face it, and it turns to face the other creature. It does not wait for the
		// turn of the other creature. On the turn after its own turn is done, it tells the other creature to taunt. It
		// tries its own taunt from the next turn.
		if (starter)
		{
			auto* other = &registry.Get<CreatureFighting>(opponent);
			if (fighting->faceOffTurns == fight::k_FaceOffStillTurn && other->part == CreatureFighting::Part::Asked)
			{
				other->part = CreatureFighting::Part::StandsStill;
			}
			if (fighting->faceOffTurns == fight::k_FaceOffOrderTurn && other->stage == CreatureFighting::Stage::FaceOff &&
			    other->faceTurn.phase == Phase::Idle)
			{
				other = obey(opponent);
				fighting = registry.TryGet<CreatureFighting>(entity);
				if (other == nullptr || fighting == nullptr)
				{
					return;
				}
				other->faceTurn.phase = Phase::Start;
			}
			if (fighting->faceOffTurns == fight::k_FaceOffOwnTurn)
			{
				fighting->faceTurn.phase = Phase::Start;
			}
			else if (fighting->faceTurn.phase == Phase::Done)
			{
				order(opponent, CreatureFighting::Stage::Taunt);
				next(CreatureFighting::Stage::Taunt);
				break;
			}
		}
		FaceOpponentTurn(registry, entity, *fighting, locomotion);
		break;
	}
	case CreatureFighting::Stage::Square:
	{
		using Phase = creature_turn_to_face::Turn::Phase;
		// The creature that started the fight tells the other creature to stop and turn to face it, then turns to face
		// the other creature. It does not wait for the turn of the other creature. On the turn after its own turn is
		// done, it tells the other creature to start its duel. It tries to start its own duel from the next turn. In
		// this stage, the played flag shows that the creature has its order to start its duel.
		if (starter)
		{
			++fighting->faceOffTurns;
			if (fighting->faceOffTurns == fight::k_SquareOrderTurn &&
			    registry.Get<const CreatureFighting>(opponent).stage == CreatureFighting::Stage::Ready)
			{
				order(opponent, CreatureFighting::Stage::Square);
				auto* other = registry.TryGet<CreatureFighting>(opponent);
				fighting = registry.TryGet<CreatureFighting>(entity);
				if (other == nullptr || fighting == nullptr)
				{
					return;
				}
				other->faceTurn = {.phase = Phase::Start};
			}
			if (fighting->faceOffTurns == fight::k_SquareOwnTurn)
			{
				fighting->faceTurn = {.phase = Phase::Start};
			}
			else if (!fighting->played && fighting->faceTurn.phase == Phase::Done)
			{
				auto* other = obey(opponent);
				fighting = registry.TryGet<CreatureFighting>(entity);
				if (other == nullptr || fighting == nullptr)
				{
					return;
				}
				other->played = true;
				fighting->played = true;
				break;
			}
		}
		// Told to start its duel, it starts it when its body lets it
		if (fighting->played)
		{
			if (CanStartDuel(registry, entity, locomotion))
			{
				BeginDuel(entity);
			}
			break;
		}
		FaceOpponentTurn(registry, entity, *fighting, locomotion);
		break;
	}
	case CreatureFighting::Stage::Taunt:
		if (!fighting->played)
		{
			// The taunt needs empty hands. A creature that holds a thing first starts to let go of it with its let-go
			// clip, and it tries the taunt from the next turn with no new check. While its body or hands are busy, it
			// cannot start to let go, and it checks again on the next turn.
			if (!fighting->handsChecked && Locator::creatureMindSystem::has_value())
			{
				const auto check = Locator::creatureMindSystem::value().CheckHandsBeforeAction(entity);
				fighting->handsChecked = check == creature_mind::HandsCheck::LetGo;
				if (check != creature_mind::HandsCheck::Free)
				{
					break;
				}
			}
			// Each try to start the taunt picks a side from the game's random numbers first. A body that plays an
			// action or moves, or that does not have the clip, refuses the start, and the creature tries again on
			// the next turn. The hands that let go keep the body busy. With no bodies loaded, nothing plays.
			const auto clip = fight::FaceOffAnimation(fighting->rude);
			const bool mirrored = Locator::gameRandom::has_value() && Locator::gameRandom::value().GameRand(2) == 1;
			const bool loaded = Locator::creatureAnimationSystem::has_value();
			const bool lettingGo = fighting->handsChecked && Locator::creatureObjectActionSystem::has_value() &&
			                       Locator::creatureObjectActionSystem::value().GetState(entity) ==
			                           CreatureObjectActionSystemInterface::State::Busy;
			if (creature_layers::IsPlaying(animation.body) || moving(entity) || lettingGo || (loaded && !Has(entity, clip)))
			{
				break;
			}
			fighting->played = true;
			fighting->taunted = loaded;
			if (loaded)
			{
				PlayOnce(animation, clip, mirrored);
			}
			fight_transcript::Note(entity, "face_off", [&] {
				return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)},
				                                 {"animation", static_cast<int32_t>(clip)},
				                                 {"played", fighting->taunted}};
			});
		}
		else if (!creature_layers::IsPlaying(animation.body))
		{
			if (!starter)
			{
				next(CreatureFighting::Stage::Ready);
			}
			else if (DrawArenaFor(registry, entity, opponent))
			{
				next(CreatureFighting::Stage::Point);
			}
			else
			{
				// With no arena the fight stops
				fight_transcript::Note(entity, "no_arena",
				                       [&] { return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)}}; });
				Leave(opponent);
				Leave(entity);
			}
		}
		break;
	case CreatureFighting::Stage::Point:
	{
		auto* hands =
		    Locator::creatureObjectActionSystem::has_value() ? &Locator::creatureObjectActionSystem::value() : nullptr;
		const bool busy = hands != nullptr && hands->GetState(entity) == CreatureObjectActionSystemInterface::State::Busy;
		if (!fighting->played)
		{
			// It points at the middle of the arena on the ground, when its hands are free of other work
			if (busy)
			{
				break;
			}
			fighting->played = true;
			if (hands != nullptr)
			{
				const auto centre = fighting->arena.centre;
				hands->PointAtFor(entity, {centre.x, GroundAt(centre), centre.y},
				                  static_cast<float>(fight::k_PointAtArenaTurns) * k_TurnSeconds, std::nullopt);
			}
		}
		else if (!busy)
		{
			if (hands != nullptr && hands->GetState(entity) != CreatureObjectActionSystemInterface::State::Idle)
			{
				hands->Cancel(entity);
			}
			order(opponent, CreatureFighting::Stage::Approach);
			next(CreatureFighting::Stage::Approach);
		}
		break;
	}
	case CreatureFighting::Stage::Approach:
		if (!fighting->played)
		{
			fighting->played = true;
			if (locomotion != nullptr)
			{
				const auto spot = fight::ArenaSpot(fighting->arena, ShownSize(body), fighting->madeArena);
				locomotion->MoveTo(entity, spot, Pace::Walk, 0.0f, fight::ArrivalDistance(ShownSize(body)));
			}
		}
		else if (!moving(entity) || fighting->stageSeconds > k_ApproachSeconds)
		{
			next(CreatureFighting::Stage::Ready);
		}
		break;
	case CreatureFighting::Stage::Ready:
		// The other creature waits for its next order. The first creature, at its place, waits for the other to arrive.
		if (!starter)
		{
			break;
		}
		// When the other creature is ready, the first creature turns to face it in the arena from the next turn
		if (partner->stage == CreatureFighting::Stage::Ready)
		{
			next(CreatureFighting::Stage::Square);
			fighting->faceOffTurns = 0;
			fighting->faceTurn = {};
		}
		else if (fighting->stageSeconds > k_ApproachSeconds)
		{
			AbortFight(entity);
		}
		break;
	case CreatureFighting::Stage::Duel:
		// An opponent with no life left loses the duel, also when its fight health is not zero
		if (opponentHere && fight::WinsByOpponentsLife(LifeOf(registry, opponent), registry.AllOf<ScriptControlled>(entity)))
		{
			WinByOpponentsLife(entity, opponent);
		}
		break;
	case CreatureFighting::Stage::Celebrate:
		// Its move finished, it leaves the fight. Its mind then does what the win made it do to the loser.
		if (fighting->fighter.state == fight::State::Idle)
		{
			Leave(entity);
		}
		break;
	case CreatureFighting::Stage::Stop:
		// Its move finished, it leaves the fight with no response
		if (fighting->fighter.state == fight::State::Idle)
		{
			Leave(entity);
		}
		break;
	case CreatureFighting::Stage::Respond:
		if (fighting->fighter.state != fight::State::Idle)
		{
			break;
		}
		// The first creature won by the life of the other creature, which stays in its duel. On the turn that its body
		// is at rest, its duel step ends. On the next turn, it orders the other creature to respond.
		if (starter && !fighting->played && together && partner->stage == CreatureFighting::Stage::Duel &&
		    partner->partnerLeft == CreatureFighting::PartnerLeft::Won)
		{
			if (!fighting->restedAfterDuel)
			{
				fighting->restedAfterDuel = true;
				break;
			}
			OrderLoserToRespond(entity, opponent);
			return;
		}
		if (!fighting->played)
		{
			fighting->played = true;
			fighting->showedResponse = true;
			const auto life = LifeOf(registry, entity);
			const auto otherLife = opponentHere ? LifeOf(registry, opponent) : 0.0f;
			// The game's random numbers choose the side of the response, one time in two the left side
			const bool mirrored = Locator::gameRandom::has_value() && Locator::gameRandom::value().GameRand(2) == 1;
			PlayOnce(animation, fight::Response(life, otherLife), mirrored);
		}
		else if (!creature_layers::IsPlaying(animation.body))
		{
			// A creature that ended its response with no fear does not look at the lives again. The plan of the first
			// creature ends at the start of the second turn after its response. The other creature continues to obey,
			// and waits until the plan of the first creature ends.
			if (fighting->responded)
			{
				++fighting->turnsAfterResponse;
				// On the first turn after its response, the first creature frees the mind of the other creature
				if (fighting->madeArena && fighting->turnsAfterResponse == 1)
				{
					if (const auto freed = PartnerAfterDuel(registry, entity, *fighting); freed.has_value())
					{
						registry.Get<CreatureFighting>(*freed).mindFree = true;
					}
				}
				// A creature that the first creature released waits until its mind chooses a new plan
				const bool ends = fighting->madeArena
				                      ? fighting->turnsAfterResponse >= fight::k_PlanEndTurnsAfterResponse
				                      : !fighting->released && !PartnerAfterDuel(registry, entity, *fighting).has_value();
				if (ends)
				{
					Leave(entity);
				}
				break;
			}
			// A fight that was called off ends with its move: the plan stopped already, so nothing follows
			if (!fighting->showedResponse)
			{
				Leave(entity);
				break;
			}
			// The creature that has no more life than the other is made to fear the other
			const bool fears = opponentHere && fight::FearsAfterFight(LifeOf(registry, entity), LifeOf(registry, opponent));
			if (!fears &&
			    (fighting->madeArena || fighting->released || PartnerAfterDuel(registry, entity, *fighting).has_value()))
			{
				fighting->responded = true;
				fighting->turnsAfterResponse = 0;
				break;
			}
			Leave(entity);
			if (fears && Locator::creatureMindSystem::has_value())
			{
				[[maybe_unused]] const bool planned =
				    Locator::creatureMindSystem::value().ForceDesireOn(entity, creature_desires::Desire::Fear, opponent);
				fight_transcript::Note(entity, "fear_after_fight", [&] {
					return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)},
					                                 {"of", fight_transcript::Id(opponent)},
					                                 {"planned", planned}};
				});
				// The fear stops what the other creature does: its response ends, and its mind is free again.
				// Thus, when both have the same life, only the first of the two to finish its response fears.
				if (const auto* other = registry.Valid(opponent) ? registry.TryGet<const CreatureFighting>(opponent) : nullptr;
				    other != nullptr && other->opponent == entity && other->stage == CreatureFighting::Stage::Respond)
				{
					fight_transcript::Note(opponent, "stopped_by_partner", [&] {
						return fight_transcript::Fields {{"creature", fight_transcript::Id(opponent)},
						                                 {"by", fight_transcript::Id(entity)}};
					});
					PartnerPlanFails(opponent, entity, other->madeArena);
					Leave(opponent);
				}
			}
		}
		break;
	}
}

void CreatureFightSystem::BeginDuel(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& fighting = registry.Get<CreatureFighting>(creature);
	NoteStage(creature, fighting.stage, CreatureFighting::Stage::Duel);
	fighting.stage = CreatureFighting::Stage::Duel;
	fighting.stageSeconds = 0.0f;
	// A fight counts once the duel starts. A fight left before it does not count.
	auto& record = registry.AllOf<CreatureFightRecord>(creature) ? registry.Get<CreatureFightRecord>(creature)
	                                                             : registry.Assign<CreatureFightRecord>(creature);
	++record.fights;
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().StopAtOnce(creature);
	}
	MeasureBlows(creature);
	fight::Enter(fighting.fighter, fight::State::Start);
	// It first turns on the spot to the heading towards its opponent, however small the turn
	const auto& opponent = fighting.opponent;
	if (Locator::creatureLocomotionSystem::has_value() && registry.Valid(opponent) && registry.AllOf<Transform>(opponent))
	{
		const auto towards =
		    Flat(registry.Get<const Transform>(opponent).position) - Flat(registry.Get<const Transform>(creature).position);
		if (glm::length(towards) > 0.0f)
		{
			fighting.fighter.turning = Locator::creatureLocomotionSystem::value().TurnToHeadingAtOnce(
			    creature, creature_locomotion::HeadingOf(towards));
		}
	}
}

void CreatureFightSystem::DuelTurn(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto random = [this](uint32_t range) { return _random.GameRand(range); };
	{
		auto* fighting = registry.TryGet<CreatureFighting>(entity);
		if (fighting == nullptr || fighting->stage != CreatureFighting::Stage::Duel)
		{
			return;
		}
		// Its opponent gone or out of the fight, it finishes. An opponent that is about to start its duel is in the fight.
		const auto opponent = fighting->opponent;
		if (!OpponentStartsDuel(registry, entity, *fighting) &&
		    (!FightsOn(registry, entity, *fighting) || registry.Get<const CreatureFighting>(opponent).opponent != entity))
		{
			// The opponent left the duel before it, and the fight of the opponent ended: it stops with no response
			if (fighting->partnerLeft != CreatureFighting::PartnerLeft::None)
			{
				StopFight(entity, ResultWhenStopped(fighting->partnerLeft), "partner_stopped");
				return;
			}
			NoteFightOver(entity, "opponent_left");
			EndFightFor(entity, true);
			NoteStage(entity, fighting->stage, CreatureFighting::Stage::Respond);
			fighting->stage = CreatureFighting::Stage::Respond;
			fighting->played = false;
			fight::Enter(fighting->fighter, fight::State::Finish);
			return;
		}
		auto& fighter = fighting->fighter;
		const auto& other = registry.Get<const CreatureFighting>(opponent).fighter;

		// Left alone long enough, the computer takes over the player's creature. A frozen creature waits longer.
		if (fight::CountComputerWait(fighter, TurnMsOf(registry.TryGet<const CreatureAnimation>(entity))))
		{
			fight_transcript::Note(entity, "control", [&] {
				return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)},
				                                 {"from", "player"},
				                                 {"to", "computer"},
				                                 {"why", "player_idle"},
				                                 {"computer_wait_ms", fighter.computerWaitMs}};
			});
		}
		const bool computer = fighter.control == fight::Control::Computer;
		const bool stance = fighter.state == fight::State::Stance;
		const bool moving = fighter.state == fight::State::Action && !fight::IsAttack(fighter.animation);
		if (fight::FacesOpponentEachTurn(fighter.state, fighter.animation, fighter.turning))
		{
			FaceOpponentNow(registry, entity, opponent);
		}
		// Near the edge of the arena, it makes no moves and steps back to the middle
		const bool kept = (stance || fighter.state == fight::State::Block) && KeepInArena(registry, entity, *fighting);
		// Bodies that overlap move apart, in its stance and in its moves other than blows
		if ((stance && !kept) || moving)
		{
			PullApartFighters(registry, entity, *fighting);
		}
		if (kept)
		{
			// It keeps inside the arena
		}
		else if (!computer)
		{
			// The computer does not look at the queue: it chooses its own moves
			if (!fighter.queue.Empty())
			{
				CheckQueue(entity);
			}
		}
		else if (fighter.state == fight::State::Stance)
		{
			const auto tier = fight::TierOf(fighter.tendency);
			const auto seen = fight::OpponentOf(other.state, other.animation);
			const auto choice = fight::ChooseMove(tier, seen, random);
			// The transcript keeps the turns on which the computer makes a move, not the turns on which it waits
			if (choice.kind != fight::Choice::Kind::None)
			{
				const auto index = fight_transcript::Note(entity, "ai_choice", [&] {
					return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)},
					                                 {"tendency", fighter.tendency},
					                                 {"tier", tier},
					                                 {"opponent_state", fight::Name(seen)},
					                                 {"choice", fight::Name(choice.kind)},
					                                 {"band", fight::Name(choice.band)},
					                                 {"step", fight::Name(choice.step)}};
				});
				if (index.has_value())
				{
					_actionCause[entity] = *index;
				}
			}
			switch (choice.kind)
			{
			case fight::Choice::Kind::Blow:
				AttemptBlow(entity, choice.band, fight::k_AiBlowSpeed);
				break;
			case fight::Choice::Kind::Block:
				fight::Enter(fighter, fight::State::BlockStart);
				break;
			case fight::Choice::Kind::Step:
				if (fight::CanStep(fighting->arena, Flat(registry.Get<const Transform>(entity).position), choice.step))
				{
					fight::Enter(fighter, fight::State::Action, fight::StepAnimation(choice.step));
				}
				break;
			case fight::Choice::Kind::None:
				break;
			}
		}
		else if (fighter.state == fight::State::Block && fight::ComputerEndsBlock(random))
		{
			const auto index = fight_transcript::Note(entity, "ai_choice", [&] {
				return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)}, {"choice", "end_block"}};
			});
			if (index.has_value())
			{
				_actionCause[entity] = *index;
			}
			fight::Enter(fighter, fight::State::BlockEnd);
		}
	}
}

void CreatureFightSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	UpdateView(gameTime.count() / 1000.0f);
	NoteActions();
}

fight::StopChainResult CreatureFightSystem::RunStops(entt::entity stopper, std::span<const fight::StopStep> steps)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.Valid(stopper) ? registry.TryGet<CreatureFighting>(stopper) : nullptr;
	if (fighting == nullptr)
	{
		return {};
	}
	const auto partner = fighting->opponent;
	auto* other = registry.Valid(partner) ? registry.TryGet<CreatureFighting>(partner) : nullptr;
	const bool together = other != nullptr && other->opponent == stopper;
	const auto result = fight::RunStopChain(
	    {
	        .duels = {together && BodyInDuel(registry, stopper), together && BodyInDuel(registry, partner)},
	        .partners = {together && fighting->partnerInMind, together && other->partnerInMind},
	    },
	    steps);
	if (together)
	{
		fighting->partnerInMind = result.after.partners[0];
		other->partnerInMind = result.after.partners[1];
	}
	if (result.gains[0] != 0 || result.gains[1] != 0)
	{
		fight_transcript::Note(stopper, "strength_gain", [&] {
			return fight_transcript::Fields {{"creature", fight_transcript::Id(stopper)},
			                                 {"partner", fight_transcript::Id(partner)},
			                                 {"creature_gains", result.gains[0]},
			                                 {"partner_gains", result.gains[1]}};
		});
		GainStrength(stopper, result.gains[0]);
		GainStrength(partner, result.gains[1]);
	}
	return result;
}

void CreatureFightSystem::PayLifeForFight(entt::entity creature, uint32_t payments, std::string_view why)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	auto* needs = fighting != nullptr && payments > 0 ? registry.TryGet<CreatureNeeds>(creature) : nullptr;
	if (needs == nullptr)
	{
		return;
	}
	const auto before = needs->needs.life;
	needs->needs.life = fight::LifeAfterPayments(before, fighting->fighter.health, payments, LifeProtected(registry, creature));
	fight_transcript::Note(creature, "level", [&] {
		auto fields = LevelChange(creature, "life", before, needs->needs.life, why);
		fields.emplace_back("life_payments", payments);
		return fields;
	});
}

void CreatureFightSystem::OrderLoserToRespond(entt::entity creature, entt::entity loser)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* lost = registry.TryGet<CreatureFighting>(loser);
	if (lost == nullptr)
	{
		return;
	}
	// The order stops what the other creature does while it is still in its duel. It gives the strength gain to the first
	// creature, which fails, and the failure stops the other creature again through the partners.
	const auto counts =
	    fight::CountsAtStop(fight::StopEnd::Stopped, {.stopperDuels = true, .partnerDuels = InDuel(registry, creature)});
	fight_transcript::Note(creature, "order_respond", [&] {
		return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
		                                 {"loser", fight_transcript::Id(loser)},
		                                 {"loser_gains", counts.gains.stopper},
		                                 {"creature_gains", counts.gains.partner},
		                                 {"loser_leaves", counts.stopperLeaves}};
	});
	GainStrength(loser, counts.gains.stopper);
	GainStrength(creature, counts.gains.partner);
	// The other creature leaves its duel and finishes its move. Its life pays each time that it leaves its duel in the stop,
	// while its body is in a state of the fight.
	NoteFightOver(loser, "ordered_to_respond");
	EndFightFor(loser, false, fight::LifePaysAtStop(lost->fighter.state) ? counts.stopperLeaves : 0U);
	NoteStage(loser, lost->stage, CreatureFighting::Stage::Respond);
	lost->stage = CreatureFighting::Stage::Respond;
	lost->stageSeconds = 0.0f;
	lost->played = false;
	lost->fighter.stopping = true;
	lost->fighter.queue.Clear();
	lost->marks.Clear();
	lost->partnerLeft = CreatureFighting::PartnerLeft::None;
	// The partners of the two minds forget each other, so the mind of the other creature is free again. The first creature
	// does not stop it when it leaves.
	lost->released = true;
	lost->mindFree = true;
	lost->partnerInMind = false;
	if (auto* asker = registry.TryGet<CreatureFighting>(creature))
	{
		asker->partnerInMind = false;
	}
	// The plan of the first creature fails, so it plays no response and leaves its fight
	PartnerPlanFails(creature, loser, true);
	Leave(creature);
	// Then the order gives the other creature the plan to obey again, to respond
	if (registry.Valid(loser) && registry.AllOf<CreatureFighting>(loser))
	{
		StopWhatItDoes(registry, loser);
		if (Locator::creatureMindSystem::has_value())
		{
			Locator::creatureMindSystem::value().FightOrdered(loser, creature);
		}
	}
}

void CreatureFightSystem::GainStrength(entt::entity creature, uint32_t times)
{
	if (!Locator::creaturePhysiologySystem::has_value())
	{
		return;
	}
	auto& physiology = Locator::creaturePhysiologySystem::value();
	for (uint32_t gain = 0; gain < times; ++gain)
	{
		physiology.GainStrengthOf(creature, "Fight");
	}
}

void CreatureFightSystem::Win(entt::entity winner, entt::entity loser, bool byBlow)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto winRecord = fight_transcript::Note(winner, "win", [&] {
		return fight_transcript::Fields {{"winner", fight_transcript::Id(winner)},
		                                 {"loser", fight_transcript::Id(loser)},
		                                 {"by_blow", byBlow},
		                                 {"winner_health", GetFightHealth(winner)},
		                                 {"loser_health", GetFightHealth(loser)}};
	});
	// The plan that the win forces on the winner makes the winner give up its action as a failure. Then the loser faints,
	// and its faint makes it give up its action again. The body of each creature can end its fight more than one time,
	// and each time its life pays again.
	const auto bodyEnds = RunStops(winner, fight::KnockOutSteps()).bodyEnds;
	const auto winnerPayments = LifePaymentsOf(registry, winner, bodyEnds[0]);
	const auto loserPayments = LifePaymentsOf(registry, loser, bodyEnds[1]);
	// The winner is made to do something to the loser at once: an evil winner has a poo on it, any other shows off. Its
	// mind does it when the winner has left the fight. This plan counts for the feedback of the player, and it is an
	// order. After a knock out by a blow the plan is not forced: it has the low priority of the order.
	if (Locator::creatureMindSystem::has_value() && registry.Valid(loser))
	{
		// The test reads the evil to good look of the body, which shows the alignment of the turn before
		const auto* look = registry.TryGet<const CreatureMorph>(winner);
		const auto alignment = look != nullptr ? look->alignmentLook.value_or(registry.Get<const Creature>(winner).alignment)
		                                       : registry.Get<const Creature>(winner).alignment;
		const std::string_view action = fight::WinnerPoos(alignment) ? "Poo" : "ShowImpressiveAnimation";
		const bool forced = fight::WinPlanForced(byBlow);
		const auto unforcedPriority =
		    forced ? std::nullopt : std::optional(fight::k_WinPlanPriority * creature_planner::k_PriorityScale);
		const bool planned =
		    Locator::creatureMindSystem::value().ForceActivity(winner, {.desire = creature_desires::Desire::ObeyPlayer,
		                                                                .activityObject = loser,
		                                                                .action = action,
		                                                                .actionObject = loser,
		                                                                .instrument = std::nullopt,
		                                                                .ordered = true,
		                                                                .comeAcross = true,
		                                                                .unforcedPriority = unforcedPriority});
		fight_transcript::Note(
		    winner, "forced_plan",
		    [&] {
			    return fight_transcript::Fields {{"creature", fight_transcript::Id(winner)},
			                                     {"desire", "obey_player"},
			                                     {"action", action},
			                                     {"object", fight_transcript::Id(loser)},
			                                     {"alignment", alignment},
			                                     {"planned", planned},
			                                     {"ordered", true},
			                                     {"forced", forced}};
		    },
		    winRecord);
	}
	NoteFightOver(winner, "knockout");
	EndFightFor(winner, true, winnerPayments);
	// A loser that did not start its duel yet is in no fight state, so its life does not pay for the fight
	EndFightFor(loser, false, loserPayments);
	if (auto* record = registry.TryGet<CreatureFightRecord>(winner))
	{
		++record->wins;
	}
	Leave(loser);
	Faint(loser);
	// Both take up no reaction until what the knock-out made them do is over
	for (const auto self : {winner, loser})
	{
		if (const auto* mind = registry.Valid(self) ? registry.TryGet<const CreatureMindState>(self) : nullptr)
		{
			registry.AssignOrReplace<CreatureAfterFight>(self, CreatureAfterFight {.agenda = mind->idle.serial});
		}
	}
	if (auto* fighting = registry.TryGet<CreatureFighting>(winner))
	{
		NoteStage(winner, fighting->stage, CreatureFighting::Stage::Celebrate);
		fighting->stage = CreatureFighting::Stage::Celebrate;
		fighting->stageSeconds = 0.0f;
		fighting->played = false;
		fight::Enter(fighting->fighter, fight::State::Finish);
		auto& won = registry.Get<CreatureAnimation>(winner);
		shown_creature_faces::ChangeFace(registry, winner, won.face, [](const creature_layers::FaceLayer& face) {
			return creature_layers::RelaxFace(face);
		});
	}
	// A fight won by a blow is seen by the living around the winner, if the winner has a player. The reaction runs at
	// once: the villagers' towns move the player's alignment by what the reaction table says of a fight won.
	const auto owner = registry.Get<const Creature>(winner).owner;
	if (fight::FightWonIsSeen(byBlow, owner != PlayerNames::NEUTRAL) && Locator::reactionSystem::has_value())
	{
		fight_transcript::Note(
		    winner, "reaction",
		    [&] {
			    return fight_transcript::Fields {{"creature", fight_transcript::Id(winner)},
			                                     {"reaction", "react_to_fight_won"},
			                                     {"player", static_cast<int32_t>(owner)}};
		    },
		    winRecord);
		// Those who take it up believe by how impressive the winner is: what it last did, its size, and how their town
		// feels about it
		Locator::reactionSystem::value().Create({.initiator = winner,
		                                         .type = Reaction::ReactToFightWon,
		                                         .player = owner,
		                                         .position = registry.Get<const Transform>(winner).position,
		                                         .onCast = true});
	}
}

void CreatureFightSystem::WinByOpponentsLife(entt::entity winner, entt::entity loser)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& fighting = registry.Get<CreatureFighting>(winner);
	const auto winRecord = fight_transcript::Note(winner, "win", [&] {
		return fight_transcript::Fields {{"winner", fight_transcript::Id(winner)},
		                                 {"loser", fight_transcript::Id(loser)},
		                                 {"by_blow", false},
		                                 {"loser_life", LifeOf(registry, loser)},
		                                 {"winner_health", fighting.fighter.health}};
	});
	// The advisors can speak first, for a creature of the player here
	if (Locator::guidanceSystem::has_value())
	{
		Locator::guidanceSystem::value().CreatureWonFight(winner);
	}
	// The living around the winner see the fight won, if the winner has a player
	const auto owner = registry.Get<const Creature>(winner).owner;
	if (owner != PlayerNames::NEUTRAL && Locator::reactionSystem::has_value())
	{
		fight_transcript::Note(
		    winner, "reaction",
		    [&] {
			    return fight_transcript::Fields {{"creature", fight_transcript::Id(winner)},
			                                     {"reaction", "react_to_fight_won"},
			                                     {"player", static_cast<int32_t>(owner)}};
		    },
		    winRecord);
		Locator::reactionSystem::value().Create({.initiator = winner,
		                                         .type = Reaction::ReactToFightWon,
		                                         .player = owner,
		                                         .position = registry.Get<const Transform>(winner).position,
		                                         .onCast = true});
	}
	auto& record = registry.AllOf<CreatureFightRecord>(winner) ? registry.Get<CreatureFightRecord>(winner)
	                                                           : registry.Assign<CreatureFightRecord>(winner);
	++record.wins;
	// Only the winner leaves the duel. It finishes the move under way and stands, and then it responds to the fight.
	// The loser stays in its duel against the winner until the fight of the winner ends.
	if (auto* lost = registry.TryGet<CreatureFighting>(loser);
	    lost != nullptr && lost->opponent == winner && lost->stage == CreatureFighting::Stage::Duel)
	{
		lost->partnerLeft = CreatureFighting::PartnerLeft::Won;
	}
	NoteFightOver(winner, "opponent_out_of_life");
	// Only the body of the winner ends its fight, one time
	EndFightFor(winner, true, LifePaymentsOf(registry, winner, 1));
	NoteStage(winner, fighting.stage, CreatureFighting::Stage::Respond);
	fighting.stage = CreatureFighting::Stage::Respond;
	fighting.played = false;
	fighting.fighter.stopping = true;
}

void CreatureFightSystem::EndFightFor(entt::entity creature, std::optional<bool> won, uint32_t lifePayments)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.TryGet<CreatureFighting>(creature);
	if (fighting == nullptr || fighting->ended)
	{
		return;
	}
	fighting->ended = true;
	const auto ended = fight_transcript::Note(creature, "fight_end", [&] {
		return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
		                                 {"result", !won.has_value() ? "none" : (*won ? "won" : "lost")},
		                                 {"health", fighting->fighter.health},
		                                 {"tendency", fighting->fighter.tendency},
		                                 {"control", fight::Name(fighting->fighter.control)}};
	});
	// A quarter of the fight health it lost comes off its life, one time for each payment
	if (auto* needs = lifePayments > 0 ? registry.TryGet<CreatureNeeds>(creature) : nullptr)
	{
		const auto before = needs->needs.life;
		needs->needs.life = fight::LifeAfterPayments(needs->needs.life, fighting->fighter.health, lifePayments,
		                                             LifeProtected(registry, creature));
		fight_transcript::Note(
		    creature, "level",
		    [&] {
			    auto fields = LevelChange(creature, "life", before, needs->needs.life, "fight_end");
			    fields.emplace_back("life_payments", lifePayments);
			    return fields;
		    },
		    ended);
	}
	auto& record = registry.AllOf<CreatureFightRecord>(creature) ? registry.Get<CreatureFightRecord>(creature)
	                                                             : registry.Assign<CreatureFightRecord>(creature);
	record.control = fighting->fighter.control;
	if (won.has_value() && Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().FoughtFight(creature, *won);
		fight_transcript::Note(
		    creature, "learning",
		    [&] {
			    return fight_transcript::Fields {
			        {"creature", fight_transcript::Id(creature)}, {"what", "fought_fight"}, {"won", *won}};
		    },
		    ended);
	}
	if (_pressed.has_value() && _pressed->creature == creature)
	{
		_pressed.reset();
	}
}

void CreatureFightSystem::Leave(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.TryGet<CreatureFighting>(creature);
	if (fighting == nullptr)
	{
		return;
	}
	auto& record = registry.AllOf<CreatureFightRecord>(creature) ? registry.Get<CreatureFightRecord>(creature)
	                                                             : registry.Assign<CreatureFightRecord>(creature);
	// Its fight health stays as the fight left it
	record.health = fighting->fighter.health;
	if (auto* animation = registry.TryGet<CreatureAnimation>(creature))
	{
		if (fighting->fighter.state != fight::State::Idle)
		{
			animation->body = {};
		}
		shown_creature_faces::ChangeFace(registry, creature, animation->face, [](const creature_layers::FaceLayer& face) {
			return creature_layers::RelaxFace(face);
		});
	}
	if (_pressed.has_value() && _pressed->creature == creature)
	{
		_pressed.reset();
	}
	if (fighting->madeArena)
	{
		ReleaseArena(registry, fighting->arenaEntity);
	}
	const auto opponent = fighting->opponent;
	const bool started = fighting->madeArena;
	const bool obeyed = !started && fighting->part == CreatureFighting::Part::Obeys;
	registry.Remove<CreatureFighting>(creature);
	// The other creature obeyed the first creature until its fight ended. Its plan to obey never ends as a success.
	if (obeyed && Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().ObeyEnded(creature, opponent);
	}
	// An opponent that it released with its last order continues without it
	auto* other = registry.Valid(opponent) ? registry.TryGet<CreatureFighting>(opponent) : nullptr;
	if (other == nullptr || other->opponent != creature || other->released)
	{
		return;
	}
	// An opponent that stayed in the duel against it stops now, with no response
	if (other->partnerLeft != CreatureFighting::PartnerLeft::None)
	{
		StopFight(opponent, ResultWhenStopped(other->partnerLeft), "partner_stopped");
	}
	// The plan of the first creature ends with its fight. This also stops the other creature after its duel, with no
	// response, and its plan to obey fails.
	else if (started && IsAfterDuel(other->stage))
	{
		StopPartnerAfterDuel(opponent, creature);
	}
}

void CreatureFightSystem::StopPartnerAfterDuel(entt::entity partner, entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.TryGet<const CreatureFighting>(partner);
	if (fighting == nullptr)
	{
		return;
	}
	const bool started = fighting->madeArena;
	fight_transcript::Note(partner, "stopped_by_partner", [&] {
		return fight_transcript::Fields {{"creature", fight_transcript::Id(partner)}, {"by", fight_transcript::Id(creature)}};
	});
	StopFight(partner, std::nullopt, "partner_stopped");
	// The other creature that left its fight gave up its plan to obey as it left
	if (started || registry.AllOf<CreatureFighting>(partner))
	{
		PartnerPlanFails(partner, creature, started);
	}
}

void CreatureFightSystem::StopFight(entt::entity creature, std::optional<bool> won, std::string_view why, uint32_t bodyEnds)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr;
	// A winner of a knock out does what the win made it do. The stop does not change its fight.
	if (fighting == nullptr || fighting->stage == CreatureFighting::Stage::Celebrate)
	{
		return;
	}
	if (IsLeadIn(fighting->stage) ||
	    (fighting->stage == CreatureFighting::Stage::Stop && fighting->fighter.state == fight::State::Idle))
	{
		Leave(creature);
		return;
	}
	if (fighting->stage == CreatureFighting::Stage::Stop)
	{
		return;
	}
	if (!fighting->ended)
	{
		NoteFightOver(creature, why);
		EndFightFor(creature, won, LifePaymentsOf(registry, creature, bodyEnds));
	}
	NoteStage(creature, fighting->stage, CreatureFighting::Stage::Stop);
	fighting->stage = CreatureFighting::Stage::Stop;
	fighting->stageSeconds = 0.0f;
	fighting->played = false;
	fighting->fighter.stopping = true;
	if (fighting->fighter.state == fight::State::Idle)
	{
		Leave(creature);
	}
}

void CreatureFightSystem::HandTookHold(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr;
	if (fighting == nullptr)
	{
		return;
	}
	fighting->held = true;
	fight_transcript::Note(creature, "held_by_hand", [&] {
		return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
		                                 {"stage", Name(fighting->stage)},
		                                 {"state", fight::Name(fighting->fighter.state)}};
	});
	if (fighting->stage != CreatureFighting::Stage::Duel)
	{
		return;
	}
	// It leaves the duel and finishes its move. Its opponent stays in the duel against it.
	if (auto* other = registry.Valid(fighting->opponent) ? registry.TryGet<CreatureFighting>(fighting->opponent) : nullptr;
	    other != nullptr && other->opponent == creature && other->stage == CreatureFighting::Stage::Duel)
	{
		other->partnerLeft = CreatureFighting::PartnerLeft::Held;
	}
	NoteFightOver(creature, "held_by_hand");
	EndFightFor(creature, std::nullopt, fight::LifePaysAtStop(fighting->fighter.state) ? 1U : 0U);
	NoteStage(creature, fighting->stage, CreatureFighting::Stage::Stop);
	fighting->stage = CreatureFighting::Stage::Stop;
	fighting->stageSeconds = 0.0f;
	fighting->played = false;
	fighting->fighter.stopping = true;
}

void CreatureFightSystem::HandLetGo(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr;
	if (fighting == nullptr)
	{
		return;
	}
	fighting->held = false;
	const auto opponent = fighting->opponent;
	// The plan of the creature starts again: it gives up its action as a failure. Its body already ended its fight at
	// the hold, but each stop ends it again, and each time its life pays again while its body is in a state of the fight.
	const auto bodyEnds = RunStops(creature, fight::StepsOf(fight::CallOffStart::Failure)).bodyEnds;
	const auto payments = LifePaymentsOf(registry, creature, bodyEnds[0]);
	fight_transcript::Note(creature, "let_go_by_hand", [&] {
		return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)}, {"stage", Name(fighting->stage)}};
	});
	if (fighting->ended)
	{
		PayLifeForFight(creature, payments, "let_go_by_hand");
	}
	// Its plan stops: its fight stops with no response, and it tells its opponent to stop too
	StopFight(creature, std::nullopt, "let_go_by_hand", bodyEnds[0]);
	if (auto* other = registry.Valid(opponent) ? registry.TryGet<CreatureFighting>(opponent) : nullptr;
	    other != nullptr && other->opponent == creature)
	{
		StopFight(opponent, ResultWhenStopped(other->partnerLeft), "partner_stopped", bodyEnds[1]);
	}
}

void CreatureFightSystem::EndBodyFightAgain(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	if (fighting == nullptr || !fighting->ended || fighting->partnerInMind)
	{
		return;
	}
	PayLifeForFight(creature, LifePaymentsOf(registry, creature, 1), "stopped_again");
}

void CreatureFightSystem::ProcessKnockedOut()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> lying;
	registry.Each<CreatureKnockedOut>(
	    [&lying](entt::entity entity, CreatureKnockedOut& /*knockedOut*/) { lying.push_back(entity); });
	for (const auto entity : lying)
	{
		KnockedOutTurn(entity);
	}
}

void CreatureFightSystem::KnockedOutTurn(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<CreatureKnockedOut>(entity))
	{
		return;
	}
	// The first turn after the knock-out starts the faint on the body. First the creature checks its hands: with a
	// thing in the hand it starts to let go, and the faint starts on the next turn and cuts the let-go. While its
	// body or hands are busy it cannot start to let go, and the faint waits.
	auto& knockedOut = registry.Get<CreatureKnockedOut>(entity);
	if (knockedOut.fainting)
	{
		const auto check = knockedOut.handsChecked || !Locator::creatureMindSystem::has_value()
		                       ? creature_mind::HandsCheck::Free
		                       : Locator::creatureMindSystem::value().CheckHandsBeforeAction(entity);
		knockedOut.handsChecked = check == creature_mind::HandsCheck::LetGo;
		if (check == creature_mind::HandsCheck::Free)
		{
			knockedOut.fainting = false;
			StartFaint(entity);
		}
	}
	auto* needs = registry.TryGet<CreatureNeeds>(entity);
	auto& animation = registry.Get<CreatureAnimation>(entity);
	const auto& body = registry.Get<const Creature>(entity);
	const fight::FaintBody faintBody {.faintStarted = !knockedOut.fainting,
	                                  .acts = creature_layers::IsPlaying(animation.body),
	                                  .lies = creature_layers::IsLooping(animation.body)};
	if (knockedOut.stage == CreatureKnockedOut::Stage::Lying)
	{
		// It counts the time that it lies only from when it lies still for the first time. A script that moves it
		// after that stands its body, but the time goes on.
		const auto wait = fight::LyingTurn({.seconds = knockedOut.seconds, .started = knockedOut.layStill}, faintBody,
		                                   k_TurnSeconds, -creature_mind::k_FaintWaitTurnsLate * k_TurnSeconds);
		knockedOut.seconds = wait.seconds;
		knockedOut.layStill = wait.started;
	}
	else if (knockedOut.stage != CreatureKnockedOut::Stage::Waiting || fight::WaitTurnCounts(faintBody))
	{
		knockedOut.seconds += k_TurnSeconds;
	}
	const auto next = [&knockedOut, entity](CreatureKnockedOut::Stage stage) {
		fight_transcript::Note(entity, "knocked_out", [&] {
			return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)},
			                                 {"from", Name(knockedOut.stage)},
			                                 {"to", Name(stage)},
			                                 {"seconds", knockedOut.seconds}};
		});
		knockedOut.stage = stage;
		knockedOut.seconds = 0.0f;
	};
	const auto getUp = [&] {
		next(CreatureKnockedOut::Stage::GettingUp);
		animation.body = creature_layers::EndLoop(animation.body);
		SetEyes(registry, entity, creature_eyes::Mode::Calm);
	};

	switch (knockedOut.stage)
	{
	case CreatureKnockedOut::Stage::Lying:
		if (!knockedOut.permanent && knockedOut.seconds >= fight::FaintSeconds(ShownSize(body)))
		{
			// A player's creature is taken home; any other comes round where it lies
			if (body.owner != PlayerNames::NEUTRAL)
			{
				// It fizzes out of sight where it lies, with the teleport's sound
				next(CreatureKnockedOut::Stage::FadingOut);
				knockedOut.fizzTurns = creature_fizz::TurnsOf(creature_fizz::k_CarryHomeSeconds, k_TurnMilliseconds);
				SetFizz(entity, 1.0f, creature_fizz::k_CarryHomeSeconds);
			}
			else
			{
				next(CreatureKnockedOut::Stage::Waiting);
			}
		}
		break;
	case CreatureKnockedOut::Stage::FadingOut:
		knockedOut.fizzTurns = knockedOut.fizzTurns > 0 ? knockedOut.fizzTurns - 1 : 0;
		if (knockedOut.fizzTurns == 0)
		{
			// Out of sight, it is moved home and fizzes back into sight there
			auto* locomotion = registry.TryGet<CreatureLocomotion>(entity);
			if (knockedOut.home.has_value() && locomotion != nullptr)
			{
				// The temple's pen is taken again now, as the home follows the temple every turn
				const auto from = registry.Get<const Transform>(entity).position;
				// The creature goes to the exact map position of its home, which the leash keeps
				const auto* leash = registry.TryGet<const CreatureLeash>(entity);
				const auto homeOnMap = leash != nullptr ? leash->homeOnMap : std::nullopt;
				auto to = creature_home::HomeOf(registry, entity).value_or(*knockedOut.home);
				if (homeOnMap.has_value())
				{
					to.x = map_coords::ToMetres(homeOnMap->x);
					to.z = map_coords::ToMetres(homeOnMap->z);
				}
				PlaceAt(*locomotion, registry.Get<Transform>(entity), to);
				ecs::thing_map_place::Keep(registry, entity, homeOnMap.value_or(map_coords::FromMetres({to.x, to.z})));
				registry.SetDirty();
				fight_transcript::Note(entity, "taken_home", [&] {
					return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)},
					                                 {"from_x", from.x},
					                                 {"from_z", from.z},
					                                 {"to_x", to.x},
					                                 {"to_z", to.z},
					                                 {"distance", glm::distance(Flat(from), Flat(to))},
					                                 {"script_controlled", registry.AllOf<ScriptControlled>(entity)}};
				});
			}
			SetFizz(entity, 0.0f, creature_fizz::k_CarryHomeSeconds);
			// Home, it is no more exhausted or thirsty than it can bear, with a little energy
			if (Locator::creaturePhysiologySystem::has_value())
			{
				Locator::creaturePhysiologySystem::value().WakeFromFaint(entity);
			}
			next(CreatureKnockedOut::Stage::FadingIn);
			knockedOut.fizzTurns = creature_fizz::TurnsOf(creature_fizz::k_CarryHomeSeconds, k_TurnMilliseconds);
		}
		break;
	case CreatureKnockedOut::Stage::FadingIn:
		knockedOut.fizzTurns = knockedOut.fizzTurns > 0 ? knockedOut.fizzTurns - 1 : 0;
		if (knockedOut.fizzTurns == 0)
		{
			next(CreatureKnockedOut::Stage::Waiting);
		}
		break;
	case CreatureKnockedOut::Stage::Waiting:
		if (knockedOut.seconds >= fight::k_WaitAfterHomeSeconds)
		{
			next(CreatureKnockedOut::Stage::WaitingForSpells);
		}
		break;
	case CreatureKnockedOut::Stage::WaitingForSpells:
	{
		// The spells on it are brought to their end each turn until none is left
		auto* spells = registry.TryGet<CreatureSpells>(entity);
		if (spells == nullptr || creature_spells::TryFinishAll(spells->spells))
		{
			if (knockedOut.rest)
			{
				next(CreatureKnockedOut::Stage::Resting);
			}
			else
			{
				getUp();
			}
		}
		break;
	}
	case CreatureKnockedOut::Stage::Resting:
	{
		// It rests until its life is full, it is not tired at all, and the rest has lasted long enough
		const auto turns = static_cast<uint32_t>(std::lround(knockedOut.seconds / k_TurnSeconds));
		if (needs == nullptr ||
		    (creature_mind::IsBetter(needs->needs.life, needs->needs.exhaustion) && turns > creature_mind::k_FewestRestTurns))
		{
			getUp();
		}
		break;
	}
	case CreatureKnockedOut::Stage::GettingUp:
		if (!creature_layers::IsPlaying(animation.body))
		{
			if (needs != nullptr)
			{
				needs->rest = CreatureNeeds::Rest::Awake;
			}
			fight_transcript::Note(entity, "got_up", [&] { return Levels(registry, entity); });
			registry.Remove<CreatureKnockedOut>(entity);
			return;
		}
		break;
	}
	if (needs != nullptr)
	{
		// Resting, its body heals; otherwise it is out cold until it gets up
		needs->rest = knockedOut.stage == CreatureKnockedOut::Stage::Resting     ? CreatureNeeds::Rest::Resting
		              : knockedOut.stage == CreatureKnockedOut::Stage::GettingUp ? CreatureNeeds::Rest::Awake
		                                                                         : CreatureNeeds::Rest::Unconscious;
	}
}
