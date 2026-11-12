/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureObjectActionSystem.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/PortableMath.h"
#include "Common/PortableRotations.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureCatch.h"
#include "Creature/CreatureFriendAgendas.h"
#include "Creature/CreatureGiving.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLead.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreaturePointing.h"
#include "Creature/CreatureReach.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureThrow.h"
#include "Creature/CreatureWalkPlanner.h"
#include "CreatureObjectActionSystemDetail.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Components/WallHug.h"
#include "ECS/CreatureCarried.h"
#include "ECS/CreaturePlacement.h"
#include "ECS/GameEndConditions.h"
#include "ECS/Map.h"
#include "ECS/MobileObjectRecord.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/PhysicsClasses.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ObjectMeasures.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/SoundRequestInterface.h"
#include "VillagerPhysics.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;
using openblack::creature_object_actions::Kind;
using openblack::creature_object_actions::Status;
using Phase = openblack::ecs::components::CreatureObjectAction::Phase;
using namespace openblack::ecs::systems::object_action_detail;

namespace
{
/// Turning to face what it throws at or points at, it tries this many times
constexpr uint32_t k_TurnAttempts = 2;

/// Nothing smaller than this counts as a distance
constexpr float k_Tiny = 1e-4f;
/// The turns in a second, as the game calculates them for the hold of a point: a second divided by the length of a turn
constexpr auto k_PointTurnsPerSecond = static_cast<uint32_t>(1000 / TimeSystemInterface::k_TurnDuration.count());

const CreatureRig* RigOf(const Creature& creature)
{
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(creature.species);
	return rigs.Contains(id) ? &*rigs.Handle(id) : nullptr;
}

/// Where the creature's head bone is as posed at the end of its newest game turn, or its position when its species has
/// no head bone
glm::vec3 HeadOf(entt::entity creature, const Transform& transform, const Creature& body)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto* points = PointsOf(body);
	if (animation == nullptr || points == nullptr || points->head >= animation->boneMatrices.size())
	{
		return transform.position;
	}
	const auto pose = creature::TurnPose(animation->newerTurnPose, animation->boneMatrices, PlacementOf(transform));
	return glm::vec3(creature::PosedBone(points->head, pose.bones, pose.placement)[3]);
}

/// Where a bone is in an animation at a time, in the creature's own space at the world's scale
std::optional<glm::vec3> BoneAt(entt::entity creature, const Transform& transform, uint32_t bone, size_t animation,
                                float timeMs)
{
	const auto at = Locator::creatureAnimationSystem::value().BoneInAnimation(creature, animation, timeMs, bone, false);
	return at.has_value() ? std::optional(*at * transform.scale) : std::nullopt;
}

/// How fast the hand moves as it tosses something away, in the model's units a second at its own size: the middle of
/// the ends of the hand's bones over the span before the toss lets go. It is measured at the creature's first toss and
/// kept until the animations follow a new body shape.
std::optional<glm::vec3> TossHand(entt::entity creature, const CreatureRig::ActionPoints& points)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* kept = KeptMeasure<TossHandVelocity>(registry, creature))
	{
		return kept->velocity;
	}
	const auto* body = registry.TryGet<const CreatureAnimation>(creature);
	if (body == nullptr)
	{
		return std::nullopt;
	}
	const auto bones = creature_throw::TossPointBones(body->skeleton.parents, points.rightHand);
	auto& animations = Locator::creatureAnimationSystem::value();
	const auto now = animations.MiddleOfBonesInAnimation(creature, creature_throw::k_Discard, points.discardMs, bones);
	const auto before = animations.MiddleOfBonesInAnimation(
	    creature, creature_throw::k_Discard, std::max(points.discardMs - creature_throw::k_HandSpeedSpanMs, 0.0f), bones);
	if (!now.has_value() || !before.has_value())
	{
		return std::nullopt;
	}
	const auto velocity = creature_throw::HandVelocity(*now, *before, creature_throw::k_HandSpeedSpanMs);
	registry.AssignOrReplace<TossHandVelocity>(
	    creature, TossHandVelocity {.velocity = velocity, .bodyRevision = BodyRevision(registry, creature)});
	return velocity;
}

/// How heavy something is, from the game's tables
std::optional<float> WeightOf(const ecs::Registry& registry, entt::entity entity)
{
	if (!Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& info = Locator::infoConstants::value();
	if (const auto* villager = registry.TryGet<const Villager>(entity))
	{
		const auto kind = static_cast<size_t>(RowOf(*villager, GVillagerInfo::Find));
		return kind < info.villager.size() ? std::optional(info.villager.at(kind).weight) : std::nullopt;
	}
	if (const auto* object = registry.TryGet<const MobileObject>(entity))
	{
		const auto* record = ecs::mobile_object_record::Of(info, *object);
		return record != nullptr ? std::optional(record->weight) : std::nullopt;
	}
	if (registry.AllOf<Ball>(entity))
	{
		return info.ball.weight;
	}
	if (const auto* creature = registry.TryGet<const Creature>(entity))
	{
		const auto row = creature::InfoRow(creature->species);
		return row < info.creature.size() ? std::optional(info.creature.at(row).weight) : std::nullopt;
	}
	return std::nullopt;
}

/// A villager picked up stops wherever it was walking
void StopWalking(ecs::Registry& registry, entt::entity entity)
{
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(entity);
}

/// A person taken into the hand is drawn in the hand from this frame on, not where its drawing put it on the land
void DrawInHand(ecs::Registry& registry, entt::entity entity)
{
	if (auto* pose = registry.TryGet<VillagerPose>(entity))
	{
		pose->drawnAt.reset();
		pose->drawnHeading.reset();
	}
}

/// The blend of the four reaching animations for a target now, mirrored when it lies to the creature's left
struct Reach
{
	bool mirrored;
	creature_reach::Blend blend;
};

Reach ReachFor(const creature_reach::Points& points, const glm::vec3& local)
{
	const bool mirrored = creature_reach::ReachesMirrored(points, local);
	return {.mirrored = mirrored, .blend = creature_reach::Solve(mirrored ? creature_reach::Mirrored(points) : points, local)};
}

/// The body stops whatever else it plays and walking, and the animations start from their beginning
void BeginPlaying(entt::entity creature, CreatureObjectAction& action, CreatureAnimation& animation)
{
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().StopAtOnce(creature);
	}
	animation.body = {};
	action.phase = Phase::Playing;
	action.timeMs = 0.0f;
	action.durationMs = 0.0f;
	for (size_t i = 0; i < action.animationCount; ++i)
	{
		action.durationMs = std::max(action.durationMs, DurationOf(creature, action.animations.at(i)));
	}
	// Whatever it does at a moment happens by the end at the latest
	action.eventMs = std::min(action.eventMs, std::max(action.durationMs - 1.0f, 0.0f));
}

/// A person or an animal that a creature's hand closes on remembers what it did and goes into its state for the hand
creature_object_actions::HoldStart IntoCreatureHand(ecs::Registry& registry, entt::entity object)
{
	const bool villager = registry.AllOf<Villager>(object);
	const bool animal = registry.AllOf<Animal>(object) && Locator::animalSystem::has_value();
	bool accepted = true;
	if (villager)
	{
		accepted = ecs::villager_physics::IntoHand(object);
	}
	else if (animal)
	{
		Locator::animalSystem::value().IntoHand(object);
	}
	return creature_object_actions::HoldStartFor(villager || animal, registry.AllOf<Creature>(object), accepted);
}
} // namespace

namespace openblack::ecs::systems::object_action_detail
{
const CreatureRig::ActionPoints* PointsOf(const Creature& creature)
{
	const auto* rig = RigOf(creature);
	return rig != nullptr && rig->actionPoints.has_value() ? &*rig->actionPoints : nullptr;
}

glm::mat4 PlacementOf(const Transform& transform)
{
	return creature::PlacementMatrix(transform.position, transform.rotation, transform.scale);
}

glm::vec3 ToLocal(const Transform& transform, const glm::vec3& world)
{
	return glm::transpose(transform.rotation) * (world - transform.position);
}

float BearingTo(const Transform& transform, const glm::vec3& target)
{
	const auto local = ToLocal(transform, target);
	const glm::vec2 ahead {0.0f, -1.0f};
	const glm::vec2 towards {local.x, local.z};
	if (glm::length(towards) < k_Tiny)
	{
		return 0.0f;
	}
	return portable_math::Acos(std::clamp(glm::dot(ahead, glm::normalize(towards)), -1.0f, 1.0f));
}

uint32_t HandBone(const CreatureRig::ActionPoints& points, const CreatureAnimation& animation, bool mirrored)
{
	return mirrored && points.rightHand < animation.mirror.size() ? animation.mirror[points.rightHand] : points.rightHand;
}

creature::RulePose TurnPoseOf(const CreatureAnimation& animation, const Transform& transform)
{
	return creature::TurnPose(animation.newerTurnPose, animation.boneMatrices, PlacementOf(transform));
}

glm::vec3 FingerEndsOf(uint32_t hand, const CreatureAnimation& animation, const creature::RulePose& pose)
{
	const auto bones = creature_throw::TossPointBones(animation.skeleton.parents, hand);
	if (bones.empty())
	{
		return glm::vec3(creature::PosedBone(hand, pose.bones, pose.placement)[3]);
	}
	glm::vec3 sum(0.0f);
	for (const auto bone : bones)
	{
		sum += glm::vec3(creature::PosedBone(bone, pose.bones, pose.placement)[3]);
	}
	return sum / static_cast<float>(bones.size());
}

std::optional<glm::vec3> FingerEndsInAnimation(entt::entity creature, uint32_t hand, size_t animation, float timeMs)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const CreatureAnimation>(creature);
	if (body == nullptr)
	{
		return std::nullopt;
	}
	const auto bones = creature_throw::TossPointBones(body->skeleton.parents, hand);
	return Locator::creatureAnimationSystem::value().MiddleOfBonesInAnimation(creature, animation, timeMs, bones);
}

std::optional<glm::vec3> HandAt(entt::entity creature, const Transform& transform, const CreatureRig::ActionPoints& points,
                                size_t animation, float timeMs)
{
	const auto at = FingerEndsInAnimation(creature, points.rightHand, animation, timeMs);
	return at.has_value() ? std::optional(*at * transform.scale) : std::nullopt;
}

uint32_t BodyRevision(const ecs::Registry& registry, entt::entity creature)
{
	const auto* morph = registry.TryGet<const CreatureMorph>(creature);
	return morph != nullptr ? morph->revision : 0;
}

std::optional<creature_reach::Points> MeasureReach(entt::entity creature, const Transform& transform,
                                                   const CreatureRig::ActionPoints& points, ReachWith with,
                                                   const std::array<size_t, creature_reach::k_CornerCount>& animations,
                                                   float timeMs)
{
	creature_reach::Points reach {};
	for (size_t i = 0; i < animations.size(); ++i)
	{
		const auto hand = with == ReachWith::FingerEnds
		                      ? HandAt(creature, transform, points, animations.at(i), timeMs)
		                      : BoneAt(creature, transform, points.rightFoot, animations.at(i), timeMs);
		if (!hand.has_value())
		{
			return std::nullopt;
		}
		reach.at(i) = *hand;
	}
	return reach;
}

float DurationOf(entt::entity creature, size_t animation)
{
	return Locator::creatureAnimationSystem::value().AnimationDuration(creature, animation).value_or(0.0f);
}

void SetSlots(CreatureObjectAction& action, std::span<const size_t> animations, std::span<const float> weights)
{
	action.animationCount = static_cast<uint8_t>(std::min(animations.size(), action.animations.size()));
	for (size_t i = 0; i < action.animationCount; ++i)
	{
		action.animations.at(i) = animations[i];
		action.weights.at(i) = weights[i];
	}
}

void Fail(CreatureObjectAction& action, std::string reason, creature_object_actions::Failure kind)
{
	action.status = Status::Failed;
	action.failure = std::move(reason);
	action.failureKind = kind;
}

void TakeInHand(ecs::Registry& registry, entt::entity creature, entt::entity object, const CreatureObjectAction& action,
                const CreatureRig::ActionPoints& points, const CreatureAnimation& animation)
{
	registry.AssignOrReplace<CreatureHeldObject>(
	    creature, CreatureHeldObject {
	                  .object = object, .mirrored = action.mirrored, .bone = HandBone(points, animation, action.mirrored)});
	registry.AssignOrReplace<HeldByCreature>(object, HeldByCreature {.creature = creature});
	// It leaves the physics with its end, not going back on the map
	if (Locator::dynamicsSystem::has_value() && Locator::dynamicsSystem::value().Find(object) != nullptr)
	{
		Locator::dynamicsSystem::value().RemoveObject(object, false, true);
	}
	IntoCreatureHand(registry, object);
	if (registry.AllOf<Villager>(object))
	{
		StopWalking(registry, object);
		DrawInHand(registry, object);
	}
}

bool OnTheMap(const ecs::Registry& registry, entt::entity thing)
{
	return ecs::InMapCells(registry, thing) && !registry.AnyOf<InHand, HeldByCreature, InPhysics, CarriedByTornado>(thing);
}

/// The heading of a creature as the game keeps it: the heading of its walk, else the heading of its drawn turn
GameHeading FacingHeading(const ecs::Registry& registry, entt::entity creature, const Transform& transform)
{
	if (const auto* walking = registry.TryGet<const CreatureLocomotion>(creature); walking != nullptr && walking->started)
	{
		return walking->heading;
	}
	return GameHeading::FromYaw(portable_math::Atan2(transform.rotation[2][0], transform.rotation[0][0]));
}

Transform AsFacing(const ecs::Registry& registry, entt::entity creature, const Transform& transform)
{
	auto facing = transform;
	if (const auto* walking = registry.TryGet<const CreatureLocomotion>(creature); walking != nullptr && walking->started)
	{
		facing.rotation = glm::mat3(portable_math::EulerAngleY(walking->heading.Yaw()));
	}
	return facing;
}

bool RefusesCreature(const ecs::Registry& registry, entt::entity object)
{
	if (const auto* block = registry.TryGet<const HanoiBlock>(object))
	{
		return block->fixed;
	}
	return registry.AllOf<CannotBePickedUp>(object);
}
} // namespace openblack::ecs::systems::object_action_detail

namespace
{
/// The hand of the flat and the high throw: the middle of the finger ends at the throw time, at the model's own size, and
/// the speed of the hand over the 100 ms before (from 0 when the throw time is nearer the start). It is measured at the
/// creature's first throw and kept until the animations follow a new body shape.
std::optional<creature_throw::HurlHands> ThrowHandsOf(entt::entity creature, const CreatureRig::ActionPoints& points)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* kept = KeptMeasure<ThrowHands>(registry, creature))
	{
		return creature_throw::HurlHands {
		    .flat = kept->flat, .high = kept->high, .flatSpeed = kept->flatSpeed, .highSpeed = kept->highSpeed};
	}
	const auto before = std::max(points.throwMs - creature_throw::k_HandSpeedSpanMs, 0.0f);
	const auto flat = FingerEndsInAnimation(creature, points.rightHand, creature_throw::k_HurlFlat, points.throwMs);
	const auto flatBefore = FingerEndsInAnimation(creature, points.rightHand, creature_throw::k_HurlFlat, before);
	const auto high = FingerEndsInAnimation(creature, points.rightHand, creature_throw::k_HurlHigh, points.throwMs);
	const auto highBefore = FingerEndsInAnimation(creature, points.rightHand, creature_throw::k_HurlHigh, before);
	if (!flat.has_value() || !flatBefore.has_value() || !high.has_value() || !highBefore.has_value())
	{
		return std::nullopt;
	}
	const creature_throw::HurlHands hands {.flat = *flat,
	                                       .high = *high,
	                                       .flatSpeed = creature_throw::HurlHandSpeed(*flat, *flatBefore),
	                                       .highSpeed = creature_throw::HurlHandSpeed(*high, *highBefore)};
	registry.AssignOrReplace<ThrowHands>(creature, ThrowHands {.flat = hands.flat,
	                                                           .high = hands.high,
	                                                           .flatSpeed = hands.flatSpeed,
	                                                           .highSpeed = hands.highSpeed,
	                                                           .bodyRevision = BodyRevision(registry, creature)});
	return hands;
}

/// At the start of a throw: the blend of the flat and the high throw, from the slope of the velocity that takes the
/// thing to the target, and what the step of each turn needs. Without the throw hands only the flat throw plays.
std::array<float, 2> StartThrow(entt::entity creature, const Transform& transform, const CreatureRig::ActionPoints& points,
                                bool mirrored, CreatureObjectAction& action)
{
	const auto hands = ThrowHandsOf(creature, points);
	action.hurlMeasured = hands.has_value();
	if (!hands.has_value())
	{
		return {1.0f, 0.0f};
	}
	const auto lift =
	    creature_throw::LiftOf(*hands, transform.position, transform.scale.x, mirrored, action.point, action.flightSeconds);
	action.hurlFlatSpeed = hands->flatSpeed;
	action.hurlHighSpeed = hands->highSpeed;
	action.hurlSpeed = lift.speed;
	action.hurlHigh = creature_throw::HurlHighWeight(*hands, lift);
	return {1.0f - action.hurlHigh, action.hurlHigh};
}

/// The point to turn to face so that a target lies in the direction of the middle of the reach. The middle is on the
/// side of the hand that reaches for the target.
glm::vec2 FacingToReach(const Transform& transform, const creature_reach::Points& reach, const glm::vec3& target, bool mirrored)
{
	const auto from = glm::xz(transform.position);
	const auto towards = glm::xz(target) - from;
	const auto centre = creature_reach::Centre(reach);
	const glm::vec2 middle {mirrored ? -centre.x : centre.x, centre.z};
	if (glm::length(middle) < k_Tiny)
	{
		return glm::xz(target);
	}
	// How far round from ahead the middle of the reach is, and so which way to face for it to lie on the target
	const glm::vec2 ahead {0.0f, -1.0f};
	const auto offset = portable_math::Atan2((ahead.x * middle.y) - (ahead.y * middle.x), glm::dot(ahead, middle));
	const auto c = portable_math::Cos(-offset);
	const auto s = portable_math::Sin(-offset);
	return from + glm::vec2((towards.x * c) - (towards.y * s), (towards.x * s) + (towards.y * c));
}

/// The reaching animations start, weighted by where the thing lies between the corners of the reach
void BeginReaching(entt::entity creature, CreatureObjectAction& action, CreatureAnimation& animation, const Reach& reach)
{
	const auto& animations =
	    action.kind == Kind::PickUp ? creature_reach::k_PickUpAnimations : creature_reach::k_DestroyAnimations;
	SetSlots(action, animations, reach.blend.weights);
	action.mirrored = reach.mirrored;
	BeginPlaying(creature, action, animation);
}

/// How high a point is above the land under it
float AboveGround(const glm::vec3& point)
{
	if (!Locator::terrainSystem::has_value())
	{
		return point.y;
	}
	return point.y - Locator::terrainSystem::value().GetAltitude(map_coords::FtoL(point.x * 65536.0f * 0.1f),
	                                                             map_coords::FtoL(point.z * 65536.0f * 0.1f));
}

/// A villager, an animal or a creature as a thing that a creature aims ahead of. None for anything else.
std::optional<creature_lead::Living> LivingOf(const ecs::Registry& registry, entt::entity thing)
{
	if (const auto* other = registry.TryGet<const Creature>(thing))
	{
		// Another creature on its way is led by the speed of its species' table
		const auto* transform = registry.TryGet<const Transform>(thing);
		if (transform == nullptr || !Locator::infoConstants::has_value())
		{
			return std::nullopt;
		}
		const auto& info = Locator::infoConstants::value();
		const auto row = creature::InfoRow(other->species);
		const auto speed = row < info.creature.size() ? static_cast<int32_t>(info.creature.at(row).speedGroup.speedDefault) : 0;
		return creature_lead::CreatureAsLiving(
		    {map_coords::ToFixed(transform->position.x), map_coords::ToFixed(transform->position.z)}, transform->position.y,
		    speed,
		    Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(thing));
	}
	if (const auto* animal = registry.TryGet<const Animal>(thing))
	{
		// The animal's place at the end of its last turn, not the place where it is drawn between two turns
		return creature_lead::Living {.mapPosition = animal->move.position,
		                              .height = animal->position.y,
		                              .speed = animal->move.speed,
		                              .gameAngle = animal->move.angle,
		                              .moved = glm::xz(animal->position) != glm::xz(animal->previousPosition)};
	}
	const auto* transform = registry.TryGet<const Transform>(thing);
	if (!registry.AllOf<Villager>(thing) || transform == nullptr)
	{
		return std::nullopt;
	}
	creature_lead::Living living {
	    .mapPosition = {map_coords::ToFixed(transform->position.x), map_coords::ToFixed(transform->position.z)},
	    .height = transform->position.y};
	if (const auto* hug = registry.TryGet<const WallHug>(thing))
	{
		// The walk keeps the map position while the villager stays where the walk put it
		if (hug->placedAt == glm::xz(transform->position))
		{
			living.mapPosition = hug->position;
		}
		living.speed = static_cast<uint16_t>(std::clamp(static_cast<int32_t>(GetSpeedStateOf(hug->speed)), 0, 0xFFFF));
		living.gameAngle = hug->gameAngle;
	}
	if (const auto* pose = registry.TryGet<const VillagerPose>(thing))
	{
		living.moved = pose->movedLastTurn;
	}
	return living;
}

/// The place where a creature aims at a thing: ahead of a villager or an animal that moved, after some seconds, and
/// the place of anything else
glm::vec3 AimAt(const std::optional<creature_lead::Living>& living, const glm::vec3& place, float seconds)
{
	return living.has_value() && living->moved ? creature_lead::WhereAfter(*living, seconds) : place;
}

/// Where a creature walks to, to get a thing in reach: ahead of a villager or an animal that moves, by the time of the
/// walk at the speed it keeps, and to the place of anything else
glm::vec3 WalkAim(entt::entity creature, const Creature& body, const Transform& transform,
                  const std::optional<creature_lead::Living>& living, const glm::vec3& place)
{
	if (!living.has_value() || !living->moved)
	{
		return place;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* walk = registry.TryGet<const CreatureLocomotion>(creature);
	const float fraction = Locator::creatureLocomotionSystem::value().KeptFraction(creature);
	// A creature with no speed has no time for a walk, so it aims at the place
	if (walk == nullptr || !(walk->speeds.run * fraction > 0.0f))
	{
		return place;
	}
	return creature_lead::WalkTo(
	    {.position = transform.position, .size = ShownSize(body), .runSpeed = walk->speeds.run, .fraction = fraction}, *living);
}

/// On its way to a thing to pick up or stomp on, on each turn the creature aims again:
/// - It cannot see the thing: it walks near to it, as it does when it stands.
/// - The thing is nearer along the ground than the middle of its reach: it walks on, and reaches when it stands. A
///   pick-up of a thing that is too high stops.
/// - Else it walks to where the thing will be. The walk continues when that place is not 10 cm or more away from
///   the place that it walks to.
void AimWhileMoving(entt::entity creature, CreatureObjectAction& action, const Creature& body, const Transform& transform)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* at = action.target.has_value() && registry.Valid(*action.target)
	                     ? registry.TryGet<const Transform>(*action.target)
	                     : nullptr;
	if (at == nullptr)
	{
		return;
	}
	const bool pickingUp = action.kind == Kind::PickUp;
	const auto points = pickingUp
	                        ? KeptReachPoints<PickUpReach>(creature, ReachWith::FingerEnds, creature_reach::k_PickUpAnimations)
	                        : KeptReachPoints<StompReach>(creature, ReachWith::Foot, creature_reach::k_DestroyAnimations);
	if (!points.has_value())
	{
		return;
	}
	const auto living = LivingOf(registry, *action.target);
	const auto place = living.has_value() ? creature_lead::PlaceOf(*living) : at->position;
	const float dx = place.x - transform.position.x;
	const float dz = place.z - transform.position.z;
	const float flatDistance = std::sqrt((dx * dx) + (dz * dz));
	const bool seen = creature_mind::CanSeeObject(transform.position, place, object_measures::Height(registry, *action.target));
	auto destination = glm::xz(place);
	float arriveWithin = 0.0f;
	if (!seen)
	{
		arriveWithin = creature_object_actions::NearWalkDistance(object_measures::TwoDRadius(registry, creature),
		                                                         object_measures::TwoDRadius(registry, *action.target));
		const auto* mind = registry.TryGet<const CreatureMindState>(creature);
		const auto* belief = mind != nullptr && pickingUp ? mind->beliefs.Find(*action.target) : nullptr;
		if (belief != nullptr)
		{
			destination = map_coords::ToMetres(belief->place);
		}
	}
	else if (flatDistance < creature_reach::MovingReach(*points, transform.scale.x))
	{
		if (pickingUp &&
		    creature_object_actions::TooHighToPickUp(std::abs(AboveGround(place) - AboveGround(transform.position)),
		                                             gutils::GetDistanceInMetres(glm::xz(transform.position), glm::xz(place))))
		{
			Fail(action, "it is too high");
		}
		return;
	}
	else
	{
		arriveWithin = creature_reach::StraightReach(*points, transform.scale.x);
		destination = glm::xz(WalkAim(creature, body, transform, living, place));
	}
	using MoveResult = CreatureLocomotionSystemInterface::MoveResult;
	// The walk keeps the pace that the creature has. The route goes to the point itself and ends early only at a thing
	// in its way within the reach. The walk continues while the creature is in its reach.
	const auto result = Locator::creatureLocomotionSystem::value().MoveTo(
	    creature, destination, CreatureLocomotionSystemInterface::Pace::Keep, 0.0f, arriveWithin);
	if (result == MoveResult::InvalidDestination)
	{
		// The action fails, and the creature stops its walk
		Locator::creatureLocomotionSystem::value().Stop(creature);
		Fail(action, "navigation failed", creature_object_actions::Failure::NoWay);
		return;
	}
	if (result == MoveResult::Started)
	{
		action.walked = true;
	}
}

/// One step of the walk or the turn to get in reach. This step occurs on each game turn, and also on the turn that the
/// action starts.
void Approach(entt::entity creature, CreatureObjectAction& action, const Creature& body, const Transform& transform,
              CreatureAnimation& animation, const CreatureHeldObject* held)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	// The turn on the spot to get the thing in reach is a part of the reach, not of the walk: the walk-up waits for it
	if (locomotion.IsMoving(creature) && creature_object_actions::NextMovingApproachStep(action.turnedToReach) ==
	                                         creature_object_actions::MovingApproachStep::WaitForTurn)
	{
		return;
	}
	// On each turn of the walk, a thing that it can no longer pick up stops the pick-up and the walk
	if (action.kind == Kind::PickUp && action.target.has_value() && registry.Valid(*action.target) &&
	    Locator::creatureObjectActionSystem::has_value() &&
	    !Locator::creatureObjectActionSystem::value().CanPickUp(creature, *action.target))
	{
		locomotion.Stop(creature);
		Fail(action, "it can't be picked up", creature_object_actions::Failure::NoWay);
		return;
	}
	// On each turn of the walk, a thing that is no longer on the map stops the stomp and the walk
	if (action.kind == Kind::Destroy && action.target.has_value() && registry.Valid(*action.target) &&
	    !OnTheMap(registry, *action.target))
	{
		locomotion.Stop(creature);
		Fail(action, "it is not on the map", creature_object_actions::Failure::NoWay);
		return;
	}
	// A walk to a thing to pick up or stomp on has no turn limit, as in the game. The walk starts again on each turn
	// while the thing is out of reach. When the thing is inside the circle of a building, or is not at the position
	// where the creature saw it last, each new walk stops at once where it started. Then the walk continues for ever,
	// and only a faint or a stronger plan stops it.
	if (locomotion.IsMoving(creature))
	{
		// On its way to a thing to pick up or stomp on, it aims at the thing again on each turn
		if (action.kind == Kind::PickUp || action.kind == Kind::Destroy)
		{
			AimWhileMoving(creature, action, body, transform);
		}
		return;
	}
	const auto* points = PointsOf(body);
	if (points == nullptr)
	{
		Fail(action, "its species has nothing to act with");
		return;
	}
	// The walk to the thing stopped because the search found no route: the action stops at once
	if (const auto* walk = registry.TryGet<const CreatureLocomotion>(creature);
	    walk != nullptr && walk->failed && action.walked)
	{
		Fail(action, "it found no route", creature_object_actions::Failure::NoRoute);
		return;
	}
	// The walk stopped because the creature is shut in. In the game, the creature clears the way at the moment that the
	// walk stops, and the step starts a new walk only on its next turn. Here the mind finds the stop on its next turn,
	// so the hands wait for that turn. Then the cleared way or the new walk starts on the same turn as in the game. When
	// nothing takes the stop by then, the walk starts again.
	if (auto* walk = registry.TryGet<CreatureLocomotion>(creature); walk != nullptr && walk->gaveUp.has_value())
	{
		if (!std::exchange(action.waitedForWay, true))
		{
			return;
		}
		walk->gaveUp.reset();
	}
	action.waitedForWay = false;

	if (action.kind == Kind::Throw || action.kind == Kind::Point)
	{
		const auto limit = action.kind == Kind::Throw ? creature_object_actions::k_ThrowTurnRadians
		                                              : creature_object_actions::k_PointTurnRadians;
		if (action.kind == Kind::Point)
		{
			// The arm points without a turn when the point is near enough ahead, as seen from the head
			const auto facing = glm::xz(transform.rotation * (-k_MeshBack));
			const auto turn = creature_pointing::TurnAngle(HeadOf(creature, transform, body), facing, action.point);
			if (std::abs(turn) >= creature_pointing::k_TurnLimit && action.attempts < k_TurnAttempts)
			{
				++action.attempts;
				action.pointTurned = true;
				locomotion.TurnToFace(creature, glm::xz(action.point));
				return;
			}
			if (!action.pointTurned)
			{
				action.mirrored =
				    creature_pointing::PointsWithLeft(turn, held != nullptr ? std::optional(held->mirrored) : std::nullopt);
			}
		}
		else if (BearingTo(transform, action.point) > limit && action.attempts < k_TurnAttempts)
		{
			++action.attempts;
			locomotion.TurnToFace(creature, glm::xz(action.point));
			return;
		}
		if (action.kind == Kind::Throw)
		{
			action.flightSeconds =
			    action.givenFlightSeconds.value_or(creature_throw::FlightTime(transform.position, action.point));
			action.mirrored = held != nullptr && held->mirrored;
			const auto weights = StartThrow(creature, transform, *points, action.mirrored, action);
			const std::array<size_t, 2> animations {creature_throw::k_HurlFlat, creature_throw::k_HurlHigh};
			SetSlots(action, animations, weights);
			action.eventMs = points->throwMs;
		}
		else
		{
			SetPointing(action, transform, body, held, true);
			action.pointMs = points->pointMs;
		}
		BeginPlaying(creature, action, animation);
		if (action.kind == Kind::Point)
		{
			// The arm plays in the time of the first pointing animation
			action.durationMs = DurationOf(creature, creature_pointing::k_Animations.front());
		}
		return;
	}

	// Walking up to something and reaching for it
	const auto* at = action.target.has_value() && registry.Valid(*action.target)
	                     ? registry.TryGet<const Transform>(*action.target)
	                     : nullptr;
	if (at == nullptr)
	{
		Fail(action, "it has gone");
		return;
	}
	const bool pickingUp = action.kind == Kind::PickUp;
	action.eventMs = pickingUp ? points->pickUpMs : points->destroyMs;
	// The corners are the kept ones that the straight reach also uses, measured in the model's own size for the body
	// shape now. They take the creature's scale each time the reach uses them. The turn to get the thing in reach keeps
	// the corners of the test before it.
	if (!action.turnedToReach || !action.reach.has_value())
	{
		action.reach = pickingUp
		                   ? KeptReachPoints<PickUpReach>(creature, ReachWith::FingerEnds, creature_reach::k_PickUpAnimations)
		                   : KeptReachPoints<StompReach>(creature, ReachWith::Foot, creature_reach::k_DestroyAnimations);
	}
	if (!action.reach.has_value())
	{
		// Not posed yet: it tries again next turn
		return;
	}
	const auto corners = creature_reach::Scaled(*action.reach, transform.scale.x);
	action.maxReach = creature_reach::MaxReach(corners);
	// The reach is measured from the way the creature faces now, which the drawn body catches up with only over a turn
	const auto facing = AsFacing(registry, creature, transform);
	// A villager or an animal that moves is reached for where it will be when the hand or foot gets there
	const auto living = LivingOf(registry, *action.target);
	const auto place = living.has_value() ? creature_lead::PlaceOf(*living) : at->position;
	const auto reachPoint =
	    AimAt(living, place, creature_lead::ReachSeconds(ShownSize(body), static_cast<int32_t>(action.eventMs)));
	// After the turn to get the thing in reach, it reaches at once
	if (std::exchange(action.turnedToReach, false))
	{
		BeginReaching(creature, action, animation, ReachFor(corners, ToLocal(facing, reachPoint)));
		return;
	}
	const auto straight =
	    pickingUp ? KeptStraightReach<PickUpReach>(creature, ReachWith::FingerEnds, creature_reach::k_PickUpAnimations)
	              : KeptStraightReach<StompReach>(creature, ReachWith::Foot, creature_reach::k_DestroyAnimations);
	if (!straight.has_value())
	{
		// Not posed yet: it tries again next turn
		return;
	}
	// The test of the reach takes the place where the thing is now
	const auto reach = ReachFor(corners, ToLocal(facing, place));
	const float dx = place.x - transform.position.x;
	const float dz = place.z - transform.position.z;
	const float flatDistance = std::sqrt((dx * dx) + (dz * dz));
	const bool seen = creature_mind::CanSeeObject(transform.position, place, object_measures::Height(registry, *action.target));
	using Step = creature_object_actions::ApproachStep;
	const auto step = creature_object_actions::NextApproachStep(seen, flatDistance, *straight, reach.blend.inRange);
	if (step == Step::Begin)
	{
		// A thing in reach that is high above the creature stops the pick-up
		if (pickingUp && creature_object_actions::TooHighToPickUp(
		                     std::abs(AboveGround(at->position) - AboveGround(transform.position)),
		                     gutils::GetDistanceInMetres(glm::xz(transform.position), glm::xz(at->position))))
		{
			Fail(action, "it is too high");
			return;
		}
		// A thing outside the corners of its reach makes it turn first, so that the middle of its reach lies on the
		// place where it reaches for the thing. Then it reaches.
		const auto aimed = ReachFor(corners, ToLocal(facing, reachPoint));
		if (!aimed.blend.inRange && locomotion.TurnToFace(creature, FacingToReach(facing, corners, reachPoint, aimed.mirrored)))
		{
			action.turnedToReach = true;
			return;
		}
		BeginReaching(creature, action, animation, aimed);
		return;
	}
	// Out of reach, it walks to the thing again. It walks to where a villager or an animal that moves will be. Not yet
	// seen, a pick-up walks to where the creature believes the thing is, and a thing in the way ends the walk further away.
	auto destination = glm::xz(WalkAim(creature, body, transform, living, place));
	float arriveWithin = *straight;
	if (step == Step::WalkNear)
	{
		destination = glm::xz(place);
		arriveWithin = creature_object_actions::NearWalkDistance(object_measures::TwoDRadius(registry, creature),
		                                                         object_measures::TwoDRadius(registry, *action.target));
		const auto* mind = registry.TryGet<const CreatureMindState>(creature);
		const auto* belief = mind != nullptr && pickingUp ? mind->beliefs.Find(*action.target) : nullptr;
		if (belief != nullptr)
		{
			destination = map_coords::ToMetres(belief->place);
		}
	}
	++action.attempts;
	using MoveResult = CreatureLocomotionSystemInterface::MoveResult;
	// The walk keeps the pace that the creature has. The route goes to the point itself and ends early only at a thing
	// in its way within the reach.
	const auto result =
	    locomotion.MoveTo(creature, destination, CreatureLocomotionSystemInterface::Pace::Keep, 0.0f, arriveWithin);
	switch (creature_cast_moves::WalkStartOf(result == MoveResult::Started, result != MoveResult::InvalidDestination))
	{
	case creature_cast_moves::WalkStart::Walking:
		break;
	case creature_cast_moves::WalkStart::TryAgain:
		// The creature cannot walk now: it asks again on the next turn
		return;
	case creature_cast_moves::WalkStart::NoWay:
		Fail(action, "navigation failed", creature_object_actions::Failure::NoWay);
		return;
	}
	action.walked = true;
}
} // namespace

bool CreatureObjectActionSystem::Start(entt::entity creature, CreatureObjectAction action)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, CreatureAnimation, Transform>(creature))
	{
		return false;
	}
	// A new action stops an eat that waits to swallow
	SwallowStoppedEat(creature);
	const auto& body = registry.Get<const Creature>(creature);
	const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
	action.status = Status::Running;
	action.phase = Phase::Approach;
	// The hold of a point counts its turns from the turn that the point starts, before a turn to the point. A point at a
	// place holds for a minimum number of turns. A point at a thing has no minimum.
	if (action.kind == Kind::Point && !action.pointsUntilTold)
	{
		const float seconds = action.pointSeconds.value_or(creature_object_actions::k_PointSeconds);
		action.holdTurns = action.target.has_value() ? creature_pointing::HoldTurns(seconds, k_PointTurnsPerSecond)
		                                             : creature_pointing::PlaceHoldTurns(seconds, k_PointTurnsPerSecond);
		action.holdCounting = false;
	}
	switch (creature_object_actions::HandsFor(action.kind))
	{
	case creature_object_actions::Hands::Holding:
		if (held == nullptr)
		{
			Fail(action, "it has nothing in its hand");
		}
		break;
	case creature_object_actions::Hands::Empty:
		if (held != nullptr)
		{
			Fail(action, "its hands are full");
		}
		break;
	case creature_object_actions::Hands::Either:
		break;
	}
	const auto* points = PointsOf(body);
	if (action.status == Status::Running && points == nullptr)
	{
		Fail(action, "its species has nothing to act with");
	}
	if (action.status == Status::Running && action.target.has_value())
	{
		if (!registry.Valid(*action.target) || !registry.AllOf<Transform>(*action.target))
		{
			Fail(action, "there is nothing there");
		}
		else if (action.kind == Kind::PickUp &&
		         (!CanPickUp(creature, *action.target) || registry.AllOf<HeldByCreature>(*action.target)))
		{
			Fail(action, "it can't be picked up", creature_object_actions::Failure::NoWay);
		}
		else if (action.kind == Kind::Destroy && !OnTheMap(registry, *action.target))
		{
			// A stomp needs the thing on the map
			Fail(action, "it is not on the map", creature_object_actions::Failure::NoWay);
		}
		else if (action.kind == Kind::Kick &&
		         ((held != nullptr && held->object == *action.target) || registry.AllOf<InHand>(*action.target)))
		{
			Fail(action, "it is in a hand");
		}
		else if (action.kind == Kind::Kick && DurationOf(creature, creature_object_actions::k_KickAnimation) <= 0.0f)
		{
			Fail(action, "its species can't kick");
		}
	}
	if (action.status == Status::Running && action.kind == Kind::Throw)
	{
		const auto& transform = registry.Get<const Transform>(creature);
		const auto ground = glm::distance(glm::xz(transform.position), glm::xz(action.point));
		if (!creature_throw::FarEnoughToThrow(ground, ShownSize(body)))
		{
			Fail(action, "too close to throw at");
		}
	}

	if (action.status == Status::Running && points != nullptr)
	{
		// Acting on what it holds starts at once, with the hand it holds it in
		const auto single = [&](size_t animation, float eventMs) {
			const std::array<size_t, 1> animations {animation};
			const std::array<float, 1> weights {1.0f};
			SetSlots(action, animations, weights);
			action.mirrored = held != nullptr && held->mirrored;
			action.eventMs = eventMs;
		};
		// The creature measures the speed of its hand for a toss when it first starts to let go of a thing, and keeps
		// that speed
		if (action.kind == Kind::PutDown || action.kind == Kind::Discard)
		{
			TossHand(creature, *points);
		}
		bool now = true;
		switch (action.kind)
		{
		case Kind::PutDown:
			single(creature_throw::k_PutDown, points->putDownMs);
			break;
		case Kind::Discard:
		{
			// The animation that puts the thing down lets go at the moment of the put-down. Any other animation lets go at
			// the moment of the toss, if it lets go.
			const auto animation = action.animations.front() != 0 ? action.animations.front() : creature_throw::k_Discard;
			single(animation, creature_throw::LetGoOf(animation) == creature_throw::LetGo::PutDownStill ? points->putDownMs
			                                                                                            : points->discardMs);
			break;
		}
		case Kind::Eat:
			single(creature_throw::k_Eat, points->eatMs);
			break;
		case Kind::Keep:
			// The animation was chosen as the action was asked for
			single(action.animations.front(), 0.0f);
			action.eventDone = true;
			break;
		case Kind::PickUp:
		case Kind::Destroy:
		case Kind::Throw:
		case Kind::Point:
			now = false;
			break;
		case Kind::Catch:
			now = StartCatch(creature, action, *points);
			break;
		case Kind::Kick:
			single(creature_object_actions::k_KickAnimation, points->kickMs);
			action.mirrored = false;
			break;
		case Kind::TakeFromHand:
		{
			// It reaches with the hand it was asked to, whatever it holds
			const bool mirrored = action.mirrored;
			single(creature_giving::k_TakeAnimation, points->takeFromHandMs);
			action.mirrored = mirrored;
			break;
		}
		}
		if (now)
		{
			BeginPlaying(creature, action, registry.Get<CreatureAnimation>(creature));
			if (action.kind == Kind::Catch)
			{
				TurnToCatch(creature, action);
			}
			// As the eat animation starts, the body takes in the food of what the hand holds
			if (action.kind == Kind::Eat && held != nullptr)
			{
				action.mealEnergy = TakeInFood(creature, FoodValueOf(held->object));
				// The scripts can read how many times the creature started to eat
				registry.Get<Creature>(creature).timesEaten += 1.0f;
			}
		}
	}
	const bool started = action.status == Status::Running;
	auto& kept = registry.AssignOrReplace<CreatureObjectAction>(creature, std::move(action));
	// The creature starts its walk or its turn to the target on the turn that the action starts. Thus another creature
	// that waits while this creature walks to a thing sees the walk at once.
	if (started && kept.phase == Phase::Approach)
	{
		Approach(creature, kept, body, registry.Get<const Transform>(creature), registry.Get<CreatureAnimation>(creature),
		         held);
	}
	return started;
}

void CreatureObjectActionSystem::Cancel(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(creature))
	{
		// A stopped eat swallows what left the hand at the bite
		SwallowStoppedEat(creature);
		registry.Remove<CreatureObjectAction>(creature);
		if (auto* animation = registry.TryGet<CreatureAnimation>(creature))
		{
			animation->slots.clear();
		}
	}
}

CreatureObjectActionSystem::State CreatureObjectActionSystem::GetState(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	if (action == nullptr)
	{
		return State::Idle;
	}
	switch (action->status)
	{
	case Status::Running:
	case Status::Contact:
		return State::Busy;
	case Status::Done:
		return State::Done;
	case Status::Failed:
		break;
	}
	return State::Failed;
}

creature_object_actions::Failure CreatureObjectActionSystem::GetFailure(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	return action != nullptr && action->status == Status::Failed ? action->failureKind
	                                                             : creature_object_actions::Failure::Other;
}

namespace
{
/// Whether the body turns on the spot for the action before it plays the animations: to get the thing in reach, to
/// throw or to point
bool TurnsForIt(const CreatureObjectAction& action)
{
	if (action.phase != Phase::Approach)
	{
		return false;
	}
	switch (action.kind)
	{
	case Kind::PickUp:
	case Kind::Destroy:
		return action.turnedToReach;
	case Kind::Throw:
		return action.attempts > 0;
	case Kind::Point:
		return action.pointTurned;
	default:
		return false;
	}
}

creature_object_actions::PlanStop PlanStopOf(const CreatureObjectAction& action)
{
	return creature_object_actions::OnPlanStop(action.kind, action.phase == Phase::Playing, TurnsForIt(action));
}

bool Runs(const CreatureObjectAction& action)
{
	return action.status == Status::Running || action.status == Status::Contact;
}
} // namespace

bool CreatureObjectActionSystem::PlaysOnAlone(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	if (action == nullptr || !Runs(*action))
	{
		return false;
	}
	const auto stop = PlanStopOf(*action);
	return action->planStopped || stop == creature_object_actions::PlanStop::PlaysOn ||
	       (stop == creature_object_actions::PlanStop::EndsPoint && action->pointEnding);
}

bool CreatureObjectActionSystem::BodyActs(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	return action != nullptr && Runs(*action) &&
	       creature_object_actions::BodyStarted(action->kind, action->phase == Phase::Playing, TurnsForIt(*action));
}

void CreatureObjectActionSystem::StopForPlan(entt::entity creature, bool stopsBody)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* action = registry.Valid(creature) ? registry.TryGet<CreatureObjectAction>(creature) : nullptr;
	if (action == nullptr || !Runs(*action))
	{
		Cancel(creature);
		return;
	}
	using creature_object_actions::PlanStop;
	switch (PlanStopOf(*action))
	{
	case PlanStop::Stops:
		Cancel(creature);
		return;
	case PlanStop::EndsPoint:
		// A stop that leaves the body alone leaves the point to hold on and end by itself
		if (stopsBody)
		{
			EndPoint(creature);
		}
		else
		{
			action->planStopped = true;
		}
		return;
	case PlanStop::PlaysOn:
		break;
	}
	action->planStopped = true;
	if (action->kind != Kind::Eat)
	{
		return;
	}
	const auto held = GetHeld(creature);
	switch (creature_object_actions::OnEatStop(action->phase == Phase::Playing, action->eaten.has_value(), held.has_value()))
	{
	case creature_object_actions::EatStop::Swallows:
		SwallowStoppedEat(creature);
		break;
	case creature_object_actions::EatStop::Deletes:
		// The hand lets go of the thing, and the thing goes out of the world. The eat animation plays on with an empty
		// hand.
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} loses {} that it was about to eat: its plan stopped",
		                    entt::to_integral(creature), entt::to_integral(*held));
		registry.Remove<CreatureHeldObject>(creature);
		registry.Remove<HeldByCreature>(*held);
		ecs::world_objects::Remove(*held);
		break;
	case creature_object_actions::EatStop::Keeps:
		break;
	}
}

std::optional<float> CreatureObjectActionSystem::GetProgress(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	if (action == nullptr || action->phase != Phase::Playing ||
	    (action->status != Status::Running && action->status != Status::Contact))
	{
		return std::nullopt;
	}
	// The time and the length are whole milliseconds, so the float divide gives the same share as the game
	if (action->kind == Kind::Catch)
	{
		const auto* legs = registry.TryGet<const CreatureLocomotion>(creature);
		const auto* body = registry.TryGet<const CreatureAnimation>(creature);
		const bool turns = legs != nullptr && legs->motion != CreatureLocomotion::Motion::Standing;
		const auto standMs = static_cast<int32_t>(DurationOf(creature, creature_layers::animations::k_Stand));
		const auto standTime = creature_walk_planner::StandTime(standMs, body != nullptr ? body->breathPhase : 0.0f);
		const float length = DurationOf(creature, creature_catch::ShareAnimation(action->catchStepped));
		const float time =
		    creature_catch::ShareTimeMs(action->catching == CreatureObjectAction::Catching::Ready,
		                                turns ? std::optional(legs->legsTimeMs) : std::nullopt, standTime, action->timeMs);
		return length > 0.0f ? std::optional(time / length) : std::nullopt;
	}
	if (creature_object_actions::SharesByStand(action->kind))
	{
		const float stand = DurationOf(creature, creature_layers::animations::k_Stand);
		return stand > 0.0f ? std::optional(action->timeMs / stand) : std::nullopt;
	}
	if ((action->kind == Kind::Point && !action->pointEnding) || action->durationMs <= 0.0f)
	{
		return 0.0f;
	}
	return std::clamp(action->timeMs / action->durationMs, 0.0f, 1.0f);
}

void CreatureObjectActionSystem::ProcessTurn()
{
	if (!Locator::creatureLocomotionSystem::has_value() || !Locator::creatureAnimationSystem::has_value())
	{
		return;
	}
	BeginTurn();
	std::vector<entt::entity> creatures;
	Locator::entitiesRegistry::value().Each<const Creature>(
	    [&creatures](entt::entity entity, const Creature& /*creature*/) { creatures.push_back(entity); });
	for (const auto creature : creatures)
	{
		CreatureTurn(creature);
	}
}

void CreatureObjectActionSystem::BeginTurn()
{
	if (!Locator::creatureLocomotionSystem::has_value() || !Locator::creatureAnimationSystem::has_value())
	{
		return;
	}
	LetGoHeldThings(std::nullopt);
}

void CreatureObjectActionSystem::CreatureTurn(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::creatureLocomotionSystem::has_value() || !Locator::creatureAnimationSystem::has_value() ||
	    !registry.Valid(entity))
	{
		return;
	}
	LetGoHeldThings(entity);
	if (const auto* held = registry.TryGet<const CreatureHeldObject>(entity); held != nullptr && !registry.Valid(held->object))
	{
		registry.Remove<CreatureHeldObject>(entity);
	}

	if (registry.AllOf<Creature, Transform, CreatureAnimation, CreatureObjectAction>(entity))
	{
		auto& action = registry.Get<CreatureObjectAction>(entity);
		if (action.status == Status::Running && action.phase == Phase::Approach)
		{
			Approach(entity, action, registry.Get<const Creature>(entity), registry.Get<const Transform>(entity),
			         registry.Get<CreatureAnimation>(entity), registry.TryGet<const CreatureHeldObject>(entity));
		}
	}

	// Carrying something heavy makes it stronger as it walks about
	if (auto* needs = registry.TryGet<CreatureNeeds>(entity); needs != nullptr && registry.AllOf<Creature>(entity))
	{
		needs->carriedWeight.reset();
		const auto held = GetCarried(entity);
		if (!held.has_value())
		{
			return;
		}
		const auto weight = WeightOf(registry, *held);
		const auto own = WeightOf(registry, entity);
		if (weight.has_value() && own.has_value() && *own > 0.0f)
		{
			needs->carriedWeight = std::clamp(*weight / *own, 0.0f, 1.0f);
		}
	}
}

namespace
{
/// The body plays the animations of the action at its time, with their weights
void PoseActionSlots(entt::entity creature, const CreatureObjectAction& action, CreatureAnimation& animation)
{
	animation.slots.clear();
	for (size_t i = 0; i < action.animationCount; ++i)
	{
		const auto duration = DurationOf(creature, action.animations.at(i));
		// Each animation holds its last frame
		const auto time = std::clamp(action.timeMs, 0.0f, std::max(duration - 1.0f, 0.0f));
		animation.slots.push_back({.animation = action.animations.at(i),
		                           .timeMs = time,
		                           .weight = action.weights.at(i),
		                           .mirrored = action.mirrored});
	}
}
} // namespace

void CreatureObjectActionSystem::ActionTurn(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	// The body's actions play on once a turn, by the time of the turn, before the body takes its pose
	const auto gameTime = std::chrono::duration<float, std::milli>(TimeSystemInterface::k_TurnDuration);
	const auto play = [&](entt::entity entity, const Creature& body, const Transform& transform, CreatureAnimation& animation,
	                      CreatureObjectAction& action) {
		if (action.phase != Phase::Playing || (action.status != Status::Running && action.status != Status::Contact))
		{
			return;
		}
		// The animations move on by the whole milliseconds that the creature lives in the turn, at the rate of its
		// size. A frozen creature lives fewer milliseconds. A hurried action doubles the step. The time that a point
		// holds is the time of the mind, and does not change.
		const auto turnMs = creature_walk_planner::TurnMs(animation.playbackScale, static_cast<int32_t>(gameTime.count()));
		const float step = static_cast<float>(creature_layers::HurriedStep(
		    creature_layers::TurnStepMs(turnMs, ShownSize(body), animation.realTime), action.hurried));
		if (action.kind == Kind::Catch)
		{
			using Catching = CreatureObjectAction::Catching;
			if (action.catching == Catching::Ready || action.catching == Catching::Stepping)
			{
				UpdateReady(registry, entity, body, transform, animation, action, step);
			}
			if (action.catching != Catching::Ready && action.catching != Catching::Stepping && action.status != Status::Done)
			{
				UpdateCatch(registry, entity, body, transform, animation, action, step);
			}
			return;
		}
		// When the thing got out of reach, the time of the reach decreases by one step each turn, with the last blend.
		// The action fails when the step of the turn is longer than the time that is left.
		if (action.withdraws)
		{
			const auto back = creature_object_actions::WithdrawStep(action.timeMs, step);
			if (!back.has_value())
			{
				Fail(action, "it got out of reach");
				animation.slots.clear();
				return;
			}
			action.timeMs = *back;
			PoseActionSlots(entity, action, animation);
			return;
		}
		const float timeBefore = action.timeMs;
		if (action.kind == Kind::Point)
		{
			action.timeMs = creature_pointing::AdvanceTime(action.timeMs, step, action.pointMs, action.pointEnding);
		}
		else if (action.kind == Kind::Throw && action.hurlMeasured)
		{
			// Near the throw time the hand moves at the speed of the throw
			const creature_throw::HurlHands hands {.flatSpeed = action.hurlFlatSpeed, .highSpeed = action.hurlHighSpeed};
			const creature_throw::HurlLift lift {.speed = action.hurlSpeed};
			action.timeMs += static_cast<float>(
			    creature_throw::HurlStepMs(hands, lift, action.hurlHigh, transform.scale.x, turnMs, static_cast<int32_t>(step),
			                               static_cast<int32_t>(action.timeMs), static_cast<int32_t>(action.eventMs)));
		}
		else
		{
			action.timeMs += step;
		}

		// Until it has hold, the reach follows what it reaches for. When the thing gets out of reach, the time of the
		// reach starts to decrease.
		if ((action.kind == Kind::PickUp || action.kind == Kind::Destroy) && !action.eventDone && action.reach.has_value())
		{
			const auto* at = action.target.has_value() && registry.Valid(*action.target)
			                     ? registry.TryGet<const Transform>(*action.target)
			                     : nullptr;
			if (at == nullptr)
			{
				Fail(action, "it has gone");
				animation.slots.clear();
				return;
			}
			// It reaches for where a villager or an animal that moves will be at the moment of the hold. The time
			// left is in whole milliseconds, from the time of the reach before this turn.
			const auto living = LivingOf(registry, *action.target);
			const auto place = living.has_value() ? creature_lead::PlaceOf(*living) : at->position;
			const auto msLeft = static_cast<int32_t>(action.eventMs) - static_cast<int32_t>(timeBefore);
			const auto aim = AimAt(living, place, creature_lead::ReachSeconds(ShownSize(body), msLeft));
			const auto local = ToLocal(AsFacing(registry, entity, transform), aim);
			const auto corners = creature_reach::Scaled(*action.reach, transform.scale.x);
			const auto reach = creature_reach::Solve(action.mirrored ? creature_reach::Mirrored(corners) : corners, local);
			// A thing that is no longer on the map, for example in the player's hand, is out of reach too
			if (!OnTheMap(registry, *action.target) || !reach.inRange)
			{
				// The reach keeps the weights of the last turn. Its time is one step less than its time before this turn.
				action.withdraws = true;
				action.timeMs = creature_object_actions::WithdrawStart(timeBefore, step);
				PoseActionSlots(entity, action, animation);
				return;
			}
			action.weights = reach.weights;
		}

		if (action.kind == Kind::Point)
		{
			// Pointing at a thing, it points wherever the thing has got to. The blend follows the point each frame.
			if (action.target.has_value() && registry.Valid(*action.target))
			{
				if (const auto* at = registry.TryGet<const Transform>(*action.target); at != nullptr)
				{
					action.point = at->position;
				}
			}
			SetPointing(action, transform, body, registry.TryGet<const CreatureHeldObject>(entity), false);
			// After the hold, the arm plays to the end of its animations
			if (action.pointEnding && action.timeMs >= action.durationMs)
			{
				action.status = Status::Done;
			}
		}
		else if (action.timeMs >= action.durationMs && (action.eventDone || action.eventMs > action.durationMs))
		{
			// Thrown into a store, it waits for the thing to come down first
			const bool flying = action.thrown.has_value() && registry.Valid(*action.thrown) &&
			                    Locator::dynamicsSystem::has_value() &&
			                    Locator::dynamicsSystem::value().IsFlying(*action.thrown);
			if (!creature_object_actions::ThrowOver(true, action.waitsForLanding, flying))
			{
				animation.slots.clear();
				return;
			}
			// An eat with no thing given fails when its animation ends
			if (action.kind == Kind::Eat && !action.swallows)
			{
				Fail(action, "it was told to eat no thing");
				animation.slots.clear();
				return;
			}
			// The creature swallows what it ate as the eat animation ends
			if (action.kind == Kind::Eat)
			{
				Swallow(entity, action, action.eaten.has_value() ? FoodValueOf(*action.eaten) : std::nullopt);
			}
			action.status = Status::Done;
		}
		if (action.status == Status::Done)
		{
			animation.slots.clear();
			return;
		}

		PoseActionSlots(entity, action, animation);
	};
	if (registry.Valid(creature) && registry.AllOf<Creature, Transform, CreatureAnimation, CreatureObjectAction>(creature))
	{
		// The hold of a point decreases by one on each turn after the turn that the point starts, also while the creature
		// turns to the point. The point ends when the count gets to zero.
		auto& action = registry.Get<CreatureObjectAction>(creature);
		if (action.kind == Kind::Point && action.status == Status::Running && !action.pointEnding &&
		    action.holdTurns.has_value() && std::exchange(action.holdCounting, true) &&
		    creature_pointing::CountHoldTurn(*action.holdTurns))
		{
			EndPoint(creature);
		}
		play(creature, registry.Get<const Creature>(creature), registry.Get<const Transform>(creature),
		     registry.Get<CreatureAnimation>(creature), registry.Get<CreatureObjectAction>(creature));
	}
	StartPendingCatches(creature);
}

void CreatureObjectActionSystem::AfterPoseTurn(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	// The spheres of the body check what they hit once a turn, after the body takes its pose
	Trample(registry, creature);

	// The moments things are taken hold of and let go of, with the hand where the body was posed at the end of its
	// newest game turn
	struct Moment
	{
		entt::entity creature;
	};
	std::vector<Moment> moments;
	if (const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	    action != nullptr && action->phase == Phase::Playing && action->status == Status::Running && !action->eventDone &&
	    !action->withdraws && action->timeMs >= action->eventMs)
	{
		moments.push_back({creature});
	}
	for (const auto& moment : moments)
	{
		auto& action = registry.Get<CreatureObjectAction>(moment.creature);
		const auto& body = registry.Get<const Creature>(creature);
		const auto& transform = registry.Get<const Transform>(creature);
		const auto& animation = registry.Get<const CreatureAnimation>(creature);
		const auto* points = PointsOf(body);
		action.eventDone = true;
		if (points == nullptr)
		{
			continue;
		}
		const auto handPosition =
		    FingerEndsOf(HandBone(*points, animation, action.mirrored), animation, TurnPoseOf(animation, transform));
		switch (action.kind)
		{
		case Kind::PickUp:
		{
			const auto object = *action.target;
			if (!registry.Valid(object) || registry.AllOf<HeldByCreature>(object))
			{
				Fail(action, "it has gone");
				break;
			}
			registry.AssignOrReplace<CreatureHeldObject>(
			    creature, CreatureHeldObject {.object = object,
			                                  .mirrored = action.mirrored,
			                                  .bone = HandBone(*points, animation, action.mirrored)});
			registry.AssignOrReplace<HeldByCreature>(object, HeldByCreature {.creature = creature});
			// A person or an animal stops what it did and is held, from the ground or out of the air. One that cannot go
			// into its state for the hand is let go where it is, and the pick-up fails.
			if (IntoCreatureHand(registry, object) == creature_object_actions::HoldStart::Refused)
			{
				registry.Remove<CreatureHeldObject>(creature);
				registry.Remove<HeldByCreature>(object);
				Fail(action, "it cannot be held");
				break;
			}
			const bool flying = Locator::dynamicsSystem::has_value() && Locator::dynamicsSystem::value().IsFlying(object);
			// Whatever it is, it leaves the map's cells while the creature holds it
			registry.Remove<MapCellResident, MapCellMover>(object);
			// A standing tree pulled from the ground creaks, at the creature
			if (registry.AllOf<Tree>(object) && !flying && Locator::soundTagSystem::has_value())
			{
				Locator::soundTagSystem::value().CreatePointSound(TreeCreak(), transform.position, false);
			}
			// Taken out of the air, it leaves the physics without going back on the map
			if (flying)
			{
				Locator::dynamicsSystem::value().RemoveObject(object, false, true);
			}
			if (registry.AllOf<Villager>(object))
			{
				StopWalking(registry, object);
				DrawInHand(registry, object);
			}
			action.status = Status::Contact;
			break;
		}
		case Kind::Destroy:
		{
			const auto target = *action.target;
			action.status = Status::Contact;
			if (!registry.Valid(target))
			{
				break;
			}
			// A building is broken about where it stands, and a rock or a piece of a building is smashed in two. The blow
			// does nothing to anything else. A living thing takes the crush from the plan of the creature on each turn
			// after the blow.
			if (registry.AnyOf<Abode, SpellDispenser>(target))
			{
				if (Locator::buildingDamageSystem::has_value())
				{
					Locator::buildingDamageSystem::value().Smash(target, creature, ShownSize(body));
				}
			}
			else if (object_physics::IsRockOrPiece(target) && Locator::dynamicsSystem::has_value())
			{
				object_physics::SmashRock(Locator::dynamicsSystem::value(), target, creature);
			}
			break;
		}
		case Kind::PutDown:
			Release(creature, glm::vec3(0.0f), ReleaseFrom::LetGo);
			break;
		case Kind::Discard:
		{
			const auto letGo = creature_throw::LetGoOf(action.animations.front());
			// The animation that puts the thing down lets go of it at rest. An animation that does not toss or put down
			// keeps the thing in the hand.
			if (letGo == creature_throw::LetGo::PutDownStill)
			{
				Release(creature, glm::vec3(0.0f), ReleaseFrom::LetGo);
				break;
			}
			if (letGo == creature_throw::LetGo::Keep)
			{
				break;
			}
			// The toss away takes the speed of the hand that the creature kept since its first toss
			const auto hand = TossHand(creature, *points);
			const auto velocity =
			    hand.has_value()
			        ? creature_throw::TossVelocity(*hand, action.mirrored, FacingHeading(registry, creature, transform),
			                                       transform.scale.x, creature_throw::k_DiscardSpeedShare)
			        : glm::vec3(0.0f);
			Release(creature, velocity, ReleaseFrom::LetGo);
			break;
		}
		case Kind::Throw:
			action.thrown = GetHeld(creature);
			Release(creature,
			        creature_throw::ReleaseVelocity(action.point, handPosition, std::max(action.flightSeconds, k_Tiny)),
			        ReleaseFrom::Thrown);
			break;
		case Kind::Eat:
		{
			const auto held = GetHeld(creature);
			if (!held.has_value())
			{
				break;
			}
			// What it holds leaves its hand at the moment of eating. Told only to eat, with no thing given, it is gone.
			// Otherwise it is out of sight until the creature swallows it as the animation ends.
			registry.Remove<CreatureHeldObject>(creature);
			if (!action.swallows)
			{
				Consume(registry, creature, *held);
				SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} takes in what it held, with no thing given",
				                    entt::to_integral(creature));
				break;
			}
			registry.AssignOrReplace<HiddenByState>(*held);
			action.eaten = *held;
			break;
		}
		case Kind::TakeFromHand:
		{
			// It takes the thing only if the player's hand still gives it; otherwise its hand closes on nothing
			const auto object = *action.target;
			if (!registry.Valid(object) || !Locator::handGrabSystem::has_value() ||
			    !Locator::handGrabSystem::value().HandOver(object))
			{
				Fail(action, "the hand no longer gives it");
				break;
			}
			TakeInHand(registry, creature, object, action, *points, animation);
			action.status = Status::Contact;
			break;
		}
		case Kind::Kick:
			KickAway(creature, body, transform, animation, *action.target);
			action.status = Status::Contact;
			break;
		case Kind::Keep:
		case Kind::Point:
		case Kind::Catch:
			break;
		}
	}
}
