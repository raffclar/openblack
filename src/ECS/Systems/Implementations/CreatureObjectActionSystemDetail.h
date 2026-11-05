/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <string>

#include <entt/core/fwd.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureReach.h"
#include "Creature/CreatureRig.h"
#include "CreatureObjectActionSystem.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

/// What the files of the creature object action system share: the measures of the body and the hand, the reach, the
/// meal, the catch and the kick helpers that more than one file uses. Not for use elsewhere.
namespace openblack::ecs::systems::object_action_detail
{
using components::Creature;
using components::CreatureAnimation;
using components::CreatureHeldObject;
using components::CreatureObjectAction;
using components::Transform;
using creature::CreatureRig;

/// The meshes look back along their z axis: ahead is -z
constexpr glm::vec3 k_MeshBack {0.0f, 0.0f, 1.0f};

const CreatureRig::ActionPoints* PointsOf(const Creature& creature);

glm::mat4 PlacementOf(const Transform& transform);

/// A point of the world in the creature's own space, kept at the world's scale
glm::vec3 ToLocal(const Transform& transform, const glm::vec3& world);

/// How far round from ahead a point is, along the ground
float BearingTo(const Transform& transform, const glm::vec3& target);

/// The hand a creature acts with: the right, or its mirror
uint32_t HandBone(const CreatureRig::ActionPoints& points, const CreatureAnimation& animation, bool mirrored);

/// The pose at the end of the newest game turn, which game rules read
creature::RulePose TurnPoseOf(const CreatureAnimation& animation, const Transform& transform);

/// Where the middle of the finger ends of a hand is, in a pose of the body. A held thing sits there, and the hand lets go
/// of it there.
glm::vec3 FingerEndsOf(uint32_t hand, const CreatureAnimation& animation, const creature::RulePose& pose);

/// Where the middle of the finger ends of a hand is in an animation at a time, in the creature's own space at its own
/// size
std::optional<glm::vec3> FingerEndsInAnimation(entt::entity creature, uint32_t hand, size_t animation, float timeMs);

/// Where the middle of the finger ends of the right hand is in an animation at a time, in the creature's own space at
/// the world's scale
std::optional<glm::vec3> HandAt(entt::entity creature, const Transform& transform, const CreatureRig::ActionPoints& points,
                                size_t animation, float timeMs);

/// The revision of the creature's body shape. A kept measure of the animations is for one revision.
uint32_t BodyRevision(const ecs::Registry& registry, entt::entity creature);

/// A kept measure of the animations, when it is correct for the body shape now. Else none, and the caller measures again.
template <typename Kept>
const Kept* KeptMeasure(const ecs::Registry& registry, entt::entity creature)
{
	const auto* kept = registry.TryGet<const Kept>(creature);
	return kept != nullptr && creature_object_actions::MeasureHolds(kept->bodyRevision, BodyRevision(registry, creature))
	           ? kept
	           : nullptr;
}

/// What a creature reaches with in its reaching animations
enum class ReachWith : uint8_t
{
	/// The middle of the finger ends of the right hand, to pick something up
	FingerEnds,
	/// The right foot bone, to strike a blow
	Foot,
};

/// Where the part it reaches with gets to in each of four reaching animations
std::optional<creature_reach::Points> MeasureReach(entt::entity creature, const Transform& transform,
                                                   const CreatureRig::ActionPoints& points, ReachWith with,
                                                   const std::array<size_t, creature_reach::k_CornerCount>& animations,
                                                   float timeMs);

/// Where the hand or foot gets to in the reaching animations, measured in the model's own size the first time it is
/// wanted, and again the first time it is wanted after the animations follow a new body shape. None before the creature
/// is posed. A measure is the same whenever it is made for the same body shape. Thus a frame that only reads uses the
/// kept measure, or makes a new measure and does not keep it, and a turn keeps the new measure.
template <typename Kept>
std::optional<creature_reach::Points> KeptReachPoints(entt::entity creature, ReachWith with,
                                                      const std::array<size_t, creature_reach::k_CornerCount>& animations,
                                                      ReachKeep keep = ReachKeep::Keep)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return std::nullopt;
	}
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	if (const auto* kept = KeptMeasure<Kept>(registry, creature))
	{
		return kept->points;
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* points = body != nullptr ? PointsOf(*body) : nullptr;
	if (points == nullptr || !Locator::creatureAnimationSystem::has_value())
	{
		return std::nullopt;
	}
	auto unscaled = *transform;
	unscaled.scale = glm::vec3(1.0f);
	const auto timeMs = with == ReachWith::FingerEnds ? points->pickUpMs : points->destroyMs;
	const auto reach = MeasureReach(creature, unscaled, *points, with, animations, timeMs);
	if (!reach.has_value())
	{
		return std::nullopt;
	}
	if (keep == ReachKeep::Keep)
	{
		registry.AssignOrReplace<Kept>(creature, Kept {.points = *reach, .bodyRevision = BodyRevision(registry, creature)});
	}
	return *reach;
}

/// How far the hand or foot gets in the reaching animations, scaled as the creature is drawn. None before the creature
/// is posed.
template <typename Kept>
std::optional<float> KeptStraightReach(entt::entity creature, ReachWith with,
                                       const std::array<size_t, creature_reach::k_CornerCount>& animations,
                                       ReachKeep keep = ReachKeep::Keep)
{
	const auto points = KeptReachPoints<Kept>(creature, with, animations, keep);
	if (!points.has_value())
	{
		return std::nullopt;
	}
	return creature_reach::StraightReach(*points, Locator::entitiesRegistry::value().Get<const Transform>(creature).scale.x);
}

float DurationOf(entt::entity creature, size_t animation);

/// Something eaten is gone: out of its home and its town first
void Consume(ecs::Registry& registry, entt::entity creature, entt::entity food);

/// The body takes in the food of a thing that it eats, and answers the energy that the food gave it
float TakeInFood(entt::entity creature, std::optional<float> value);

/// The creature swallows what it ate. A thing it can't swallow it brings back up. Otherwise the meal is counted, and
/// the creature keeps it down or brings it up.
void Swallow(entt::entity creature, CreatureObjectAction& action, std::optional<float> value);

/// The foot meets what the creature kicks. The order of the rules is:
/// 1. A building breaks with two blows when it is fully built and not on fire.
/// 2. A thing that can fly flies off ahead of the creature. A thing in the air takes the new speed. A thing on the land
///    goes into the air from where it lies, from the player of the creature.
/// 3. Another creature takes a tenth of a hit, and more as the kicker is tall for it.
void KickAway(entt::entity creature, const Creature& body, const Transform& transform, const CreatureAnimation& animation,
              entt::entity target);

void SetSlots(CreatureObjectAction& action, std::span<const size_t> animations, std::span<const float> weights);

void Fail(CreatureObjectAction& action, std::string reason,
          creature_object_actions::Failure kind = creature_object_actions::Failure::Other);

/// The thing a creature catches is taken into its hand out of the air: it leaves the physics without going back on the
/// map, and a person or animal is held
void TakeInHand(ecs::Registry& registry, entt::entity creature, entt::entity object, const CreatureObjectAction& action,
                const CreatureRig::ActionPoints& points, const CreatureAnimation& animation);

/// Pointing: the four pointing animations blended by where the point is across and up from the top of the creature.
/// The arm is chosen as it starts to point, and stays the same until the point ends.
void SetPointing(CreatureObjectAction& action, const Transform& transform, const Creature& body, const CreatureHeldObject* held,
                 bool choosesArm);

/// Whether a thing lies on the map, where a creature can stomp on it: not held, flying or carried off.
/// A piece that broke off a building goes into the cells of the map when it comes to rest, as a rock does.
bool OnTheMap(const ecs::Registry& registry, entt::entity thing);

/// One game turn of walking or turning up to what the creature acts on
/// Where the creature stands and faces now: the heading its walking last reached, which the drawn body only catches up
/// with over the turn, so that a catch weighs the thing against the way the creature has finished turning
Transform AsFacing(const ecs::Registry& registry, entt::entity creature, const Transform& transform);

/// One of the three creaks of a tree a creature pulls from the ground, drawn from the local random numbers
entt::id_type TreeCreak();

/// The catch starts with the four catching animations, the thing flying across the land fast enough: the creature turns
/// to face it first when it comes from too far round, and where its hand closes in each animation is measured
bool StartCatch(entt::entity creature, CreatureObjectAction& action, const CreatureRig::ActionPoints& points);

/// A catch's creature faces back along the thing's flight, turning when it comes from too far round. It is the last thing
/// the catch's start does, once the body has stopped what else it played and its walking.
void TurnToCatch(entt::entity creature, const CreatureObjectAction& action);

/// Ready to catch: once it has turned, it waits for the thing, steps across to where it will pass when that is out of
/// reach, and starts the catch when the thing will arrive as the hand closes; behind it or too late, it gives up. The
/// step plays to its end whatever the thing does, and the creature is ready again in the frame it ends.
void UpdateReady(ecs::Registry& registry, entt::entity creature, const Creature& body, const Transform& drawn,
                 CreatureAnimation& animation, CreatureObjectAction& action, float step);

/// Each frame of a catch: the four animations blended by where the thing is against where the hand closes in each, the
/// catch played on while reaching or once caught and played back after a miss, the hand closing on it at its moment
/// unless the blend had to lean too far, and the catch over at either end
void UpdateCatch(ecs::Registry& registry, entt::entity creature, const Creature& body, const Transform& transform,
                 CreatureAnimation& animation, CreatureObjectAction& action, float step);

/// Whether the thing refuses a creature's hands. A piece of the beach temple puzzle refuses them only when the puzzle is
/// solved. The other rules of the puzzle (a base, a covered ring, a puzzle that is not calm or that is gone) stop only
/// the player's hand.
bool RefusesCreature(const ecs::Registry& registry, entt::entity object);

/// The spheres round the body of each creature come on and go off as its body actions start. While they are on, each
/// part that moved far enough since its last check knocks about what it reaches.
void Trample(ecs::Registry& registry, entt::entity only);
} // namespace openblack::ecs::systems::object_action_detail
