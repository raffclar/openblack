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
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureReach.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureThrow.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/PhysicsDrawPose.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreaturePhysics.h"
#include "ECS/CreaturePose.h"
#include "ECS/Life.h"
#include "ECS/LivingPhysics.h"
#include "ECS/MapCells.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Trees.h"
#include "ECS/VillagerSpeed.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;
using openblack::creature_object_actions::Kind;
using openblack::creature_object_actions::Status;
using Phase = openblack::ecs::components::CreatureObjectAction::Phase;

namespace
{
constexpr float k_TurnSeconds = game_clock::k_TurnSeconds;
constexpr auto k_TurnMs = static_cast<float>(game_clock::k_MsPerTurn);
/// The meshes look back along their z axis: ahead is -z
constexpr glm::vec3 k_MeshBack {0.0f, 0.0f, 1.0f};
/// It walks or turns this many times to get something in reach before giving up
constexpr uint32_t k_ReachAttempts = 4;
/// Turning to face what it throws at or points at, it tries this many times
constexpr uint32_t k_TurnAttempts = 2;
/// Walking up to something it walks to within this share of the distance to the middle of its reach, and stops no
/// nearer than this share of it
constexpr float k_ApproachFar = 1.0f;
constexpr float k_ApproachNear = 0.6f;
/// Near enough to reach once turned: within this share of the distance to the middle of its reach, either way
constexpr float k_TurnInReachShare = 0.5f;
/// Pointing, the high animation counts fully this far above level, in radians
constexpr float k_PointHighRadians = std::numbers::pi_v<float> / 4.0f;
/// Pointing, the hand is about this share of the creature's height up
constexpr float k_PointFromHeightShare = 0.6f;
/// Nothing smaller than this counts as a distance
constexpr float k_Tiny = 1e-4f;

const CreatureRig* RigOf(const Creature& creature)
{
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(creature.species);
	return rigs.Contains(id) ? &*rigs.Handle(id) : nullptr;
}

const CreatureRig::ActionPoints* PointsOf(const Creature& creature)
{
	const auto* rig = RigOf(creature);
	return rig != nullptr && rig->actionPoints.has_value() ? &*rig->actionPoints : nullptr;
}

glm::mat4 PlacementOf(const Transform& transform)
{
	return creature::PlacementMatrix(transform.position, transform.rotation, transform.scale);
}

/// A point of the world in the creature's own space, kept at the world's scale
glm::vec3 ToLocal(const Transform& transform, const glm::vec3& world)
{
	return glm::transpose(transform.rotation) * (world - transform.position);
}

/// How far round from ahead a point is, along the ground
float BearingTo(const Transform& transform, const glm::vec3& target)
{
	const auto local = ToLocal(transform, target);
	const glm::vec2 ahead {0.0f, -1.0f};
	const glm::vec2 towards {local.x, local.z};
	if (glm::length(towards) < k_Tiny)
	{
		return 0.0f;
	}
	return std::acos(std::clamp(glm::dot(ahead, glm::normalize(towards)), -1.0f, 1.0f));
}

/// The hand a creature acts with: the right, or its mirror
uint32_t HandBone(const CreatureRig::ActionPoints& points, const CreatureAnimation& animation, bool mirrored)
{
	return mirrored && points.rightHand < animation.mirror.size() ? animation.mirror[points.rightHand] : points.rightHand;
}

/// Where the palm is: among the hand bone and the fingers that hang from it, as the body is posed this frame
glm::vec3 PalmOf(uint32_t hand, const CreatureAnimation& animation, const glm::mat4& placement)
{
	auto sum = glm::vec3(creature::PosedBone(hand, animation.boneMatrices, placement)[3]);
	float count = 1.0f;
	const auto& parents = animation.skeleton.parents;
	for (uint32_t bone = 0; bone < parents.size(); ++bone)
	{
		if (parents[bone] == hand)
		{
			sum += glm::vec3(creature::PosedBone(bone, animation.boneMatrices, placement)[3]);
			count += 1.0f;
		}
	}
	return sum / count;
}

/// Where the middle of something is from where it stands, by its mesh's bounds, in its own space at the world's scale
glm::vec3 MiddleOf(const ecs::Registry& registry, entt::entity entity)
{
	const auto* mesh = registry.TryGet<const Mesh>(entity);
	const auto* transform = registry.TryGet<const Transform>(entity);
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (mesh == nullptr || transform == nullptr || !meshes.Contains(mesh->id))
	{
		return glm::vec3(0.0f);
	}
	return meshes.Handle(mesh->id)->GetBoundingBox().Center() * transform->scale;
}

/// Where the right hand is in an animation at a time, in the creature's own space at the world's scale
std::optional<glm::vec3> HandAt(entt::entity creature, const Transform& transform, const CreatureRig::ActionPoints& points,
                                size_t animation, float timeMs)
{
	if (!Locator::creatureAnimationSystem::has_value())
	{
		return std::nullopt;
	}
	const auto bone =
	    Locator::creatureAnimationSystem::value().BoneInAnimation(creature, animation, timeMs, points.rightHand, false);
	return bone.has_value() ? std::optional(*bone * transform.scale) : std::nullopt;
}

/// Where the palm is at a moment of the action's animations, as the body plays them blended, in the world: the hand
/// bone and the fingers that hang from it, in each animation at that time, weighed as the animations are. Nothing when
/// the creature hasn't been posed.
std::optional<glm::vec3> PalmAtMoment(entt::entity creature, const CreatureObjectAction& action,
                                      const CreatureRig::ActionPoints& points, const CreatureAnimation& animation,
                                      const Transform& transform)
{
	if (!Locator::creatureAnimationSystem::has_value() || action.animationCount == 0)
	{
		return std::nullopt;
	}
	auto& animations = Locator::creatureAnimationSystem::value();
	const auto hand = HandBone(points, animation, action.mirrored);
	std::vector<uint32_t> bones {hand};
	for (uint32_t bone = 0; bone < animation.skeleton.parents.size(); ++bone)
	{
		if (animation.skeleton.parents[bone] == hand)
		{
			bones.push_back(bone);
		}
	}
	glm::vec3 sum {0.0f};
	float weights = 0.0f;
	for (size_t i = 0; i < action.animationCount; ++i)
	{
		glm::vec3 palm {0.0f};
		float count = 0.0f;
		for (const auto bone : bones)
		{
			if (const auto at =
			        animations.BoneInAnimation(creature, action.animations.at(i), action.eventMs, bone, action.mirrored))
			{
				palm += *at;
				count += 1.0f;
			}
		}
		if (count > 0.0f)
		{
			sum += (palm / count) * action.weights.at(i);
			weights += action.weights.at(i);
		}
	}
	if (weights <= 0.0f)
	{
		return std::nullopt;
	}
	return glm::vec3(PlacementOf(transform) * glm::vec4(sum / weights, 1.0f));
}

/// Where the hand takes hold in each of four reaching animations
std::optional<creature_reach::Points> MeasureReach(entt::entity creature, const Transform& transform,
                                                   const CreatureRig::ActionPoints& points,
                                                   const std::array<size_t, creature_reach::k_CornerCount>& animations,
                                                   float timeMs)
{
	creature_reach::Points reach {};
	for (size_t i = 0; i < animations.size(); ++i)
	{
		const auto hand = HandAt(creature, transform, points, animations.at(i), timeMs);
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
	if (!Locator::creatureAnimationSystem::has_value())
	{
		return 0.0f;
	}
	return Locator::creatureAnimationSystem::value().AnimationDuration(creature, animation).value_or(0.0f);
}

bool IsVillager(const ecs::Registry& registry, std::optional<entt::entity> entity)
{
	return entity.has_value() && registry.Valid(*entity) && registry.AllOf<Villager>(*entity);
}

/// How heavy something is, from the game's tables
std::optional<float> WeightOf(const ecs::Registry& registry, entt::entity entity)
{
	if (!Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& info = Locator::infoConstants::value();
	if (registry.AllOf<Villager>(entity))
	{
		const auto* villager = ecs::VillagerInfoOf(entity);
		return villager != nullptr ? std::optional(villager->weight) : std::nullopt;
	}
	if (const auto* animal = registry.TryGet<const Animal>(entity))
	{
		const auto kind = static_cast<size_t>(animal->type);
		return kind < info.animal.size() ? std::optional(info.animal.at(kind).weight) : std::nullopt;
	}
	if (const auto* object = registry.TryGet<const MobileObject>(entity))
	{
		const auto kind = static_cast<size_t>(object->type);
		return kind < info.mobileObject.size() ? std::optional(info.mobileObject.at(kind).weight) : std::nullopt;
	}
	if (const auto* creature = registry.TryGet<const Creature>(entity))
	{
		const auto row = creature::InfoRow(creature->species);
		return row < info.creature.size() ? std::optional(info.creature.at(row).weight) : std::nullopt;
	}
	return std::nullopt;
}

/// Something eaten is gone: a villager or an animal dies, which takes it out of its home, its town and its flock;
/// anything else is deleted
void Consume(const ecs::Registry& registry, entt::entity food)
{
	if (registry.AnyOf<Villager, Animal>(food))
	{
		ecs::life::Kill(food, "eaten by a creature");
	}
	else
	{
		ecs::ToBeDeleted(food);
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

void SetSlots(CreatureObjectAction& action, std::span<const size_t> animations, std::span<const float> weights)
{
	action.animationCount = static_cast<uint8_t>(std::min(animations.size(), action.animations.size()));
	for (size_t i = 0; i < action.animationCount; ++i)
	{
		action.animations.at(i) = animations[i];
		action.weights.at(i) = weights[i];
	}
}

/// The body stops whatever else it plays and walking, and the animations start from their beginning
void BeginPlaying(entt::entity creature, CreatureObjectAction& action, CreatureAnimation& animation)
{
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
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

void Fail(CreatureObjectAction& action, std::string reason)
{
	action.status = Status::Failed;
	action.failure = std::move(reason);
}

/// The flat and high throws blended by how high the target is against where the hand lets go in each
std::array<float, 2> ThrowWeights(entt::entity creature, const Transform& transform, const CreatureRig::ActionPoints& points,
                                  const glm::vec3& target)
{
	const auto slope = [](const glm::vec3& local) { return local.y / std::max(-local.z, k_Tiny); };
	const auto flat = HandAt(creature, transform, points, creature_throw::k_HurlFlat, points.throwMs);
	const auto high = HandAt(creature, transform, points, creature_throw::k_HurlHigh, points.throwMs);
	if (!flat.has_value() || !high.has_value())
	{
		return {1.0f, 0.0f};
	}
	const auto weight = creature_throw::HighThrowWeight(slope(ToLocal(transform, target)), slope(*flat), slope(*high));
	return {1.0f - weight, weight};
}

/// Pointing: low and high to the side the target is on, or with the hand that is free, blended by how high it is
void SetPointing(CreatureObjectAction& action, const Transform& transform, const Creature& body, const CreatureHeldObject* held)
{
	const auto local = ToLocal(transform, action.point);
	// The left side is mirrored in the meshes' space: the right is +x
	const bool right = held != nullptr ? held->mirrored : local.x >= 0.0f;
	const auto height = creature_throw::k_HeightAtSizeOne * body.size * k_PointFromHeightShare;
	const auto elevation = std::atan2(local.y - height, std::max(glm::length(glm::vec2(local.x, local.z)), k_Tiny));
	const auto high = std::clamp(elevation / k_PointHighRadians, 0.0f, 1.0f);
	const std::array<size_t, 2> animations {
	    creature_object_actions::k_PointAnimations.at(right ? 1 : 0),
	    creature_object_actions::k_PointAnimations.at(right ? 3 : 2),
	};
	const std::array<float, 2> weights {1.0f - high, high};
	SetSlots(action, animations, weights);
	action.mirrored = false;
}

/// A vector along the ground turned by an angle
glm::vec2 Turned(glm::vec2 v, float angle)
{
	const auto c = std::cos(angle);
	const auto s = std::sin(angle);
	return {(v.x * c) - (v.y * s), (v.x * s) + (v.y * c)};
}

/// The point to turn to face so that a target lies in the direction of the middle of the reach, on whichever side
/// needs the smaller turn
glm::vec2 FacingToReach(const Transform& transform, const creature_reach::Points& reach, const glm::vec3& target)
{
	const auto from = glm::xz(transform.position);
	const auto towards = glm::xz(target) - from;
	const auto centre = creature_reach::Centre(reach);
	const glm::vec2 ahead {0.0f, -1.0f};
	const auto aheadInWorld = glm::xz(transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
	std::optional<glm::vec2> best;
	float bestTurn = 0.0f;
	for (const auto side : {1.0f, -1.0f})
	{
		const glm::vec2 middle {centre.x * side, centre.z};
		if (glm::length(middle) < k_Tiny)
		{
			continue;
		}
		// How far round from ahead the middle of the reach is, and so which way to face for it to lie on the target
		const auto offset = std::atan2((ahead.x * middle.y) - (ahead.y * middle.x), glm::dot(ahead, middle));
		const auto facing = Turned(towards, -offset);
		const auto turn = std::acos(std::clamp(glm::dot(glm::normalize(facing), glm::normalize(aheadInWorld)), -1.0f, 1.0f));
		if (!best.has_value() || turn < bestTurn)
		{
			best = facing;
			bestTurn = turn;
		}
	}
	return from + best.value_or(towards);
}

/// One game turn of walking or turning up to what the creature acts on
void Approach(entt::entity creature, CreatureObjectAction& action, const Creature& body, const Transform& transform,
              CreatureAnimation& animation, const CreatureHeldObject* held)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	if (locomotion.IsMoving(creature))
	{
		return;
	}
	const auto* points = PointsOf(body);
	if (points == nullptr)
	{
		Fail(action, "its species has nothing to act with");
		return;
	}

	if (action.kind == Kind::Throw || action.kind == Kind::Point)
	{
		const auto limit = action.kind == Kind::Throw ? creature_object_actions::k_ThrowTurnRadians
		                                              : creature_object_actions::k_PointTurnRadians;
		if (BearingTo(transform, action.point) > limit && action.attempts < k_TurnAttempts)
		{
			++action.attempts;
			locomotion.TurnToFace(creature, glm::xz(action.point));
			return;
		}
		if (action.kind == Kind::Throw)
		{
			action.flightSeconds = creature_throw::FlightTime(glm::distance(transform.position, action.point));
			const auto weights = ThrowWeights(creature, transform, *points, action.point);
			const std::array<size_t, 2> animations {creature_throw::k_HurlFlat, creature_throw::k_HurlHigh};
			SetSlots(action, animations, weights);
			action.mirrored = held != nullptr && held->mirrored;
			action.eventMs = points->throwMs;
		}
		else
		{
			SetPointing(action, transform, body, held);
			action.holdMs = creature_object_actions::k_PointSeconds * 1000.0f;
		}
		BeginPlaying(creature, action, animation);
		return;
	}

	// Walking up to something and reaching for it
	const auto* at = action.target.has_value() && ecs::IsAvailable(*action.target)
	                     ? registry.TryGet<const Transform>(*action.target)
	                     : nullptr;
	if (at == nullptr)
	{
		Fail(action, "it has gone");
		return;
	}
	if (!action.reach.has_value())
	{
		const bool pickingUp = action.kind == Kind::PickUp;
		action.eventMs = pickingUp ? points->pickUpMs : points->destroyMs;
		action.reach =
		    MeasureReach(creature, transform, *points,
		                 pickingUp ? creature_reach::k_PickUpAnimations : creature_reach::k_DestroyAnimations, action.eventMs);
		if (!action.reach.has_value())
		{
			// Not posed yet: it tries again next turn
			return;
		}
		action.maxReach = creature_reach::MaxReach(*action.reach);
	}
	const auto local = ToLocal(transform, at->position);
	const auto reach = ReachFor(*action.reach, local);
	if (reach.blend.inRange)
	{
		const auto& animations =
		    action.kind == Kind::PickUp ? creature_reach::k_PickUpAnimations : creature_reach::k_DestroyAnimations;
		SetSlots(action, animations, reach.blend.weights);
		action.mirrored = reach.mirrored;
		BeginPlaying(creature, action, animation);
		return;
	}
	if (action.attempts >= k_ReachAttempts)
	{
		Fail(action, "it couldn't get in reach");
		return;
	}
	++action.attempts;
	const auto centre = creature_reach::Centre(*action.reach);
	const auto ideal = std::max(glm::length(glm::vec2(centre.x, centre.z)), k_Tiny);
	const auto distance = glm::length(glm::vec2(local.x, local.z));
	// Near enough, it only needs to turn so that it lies where its reach is middling
	if (std::abs(distance - ideal) <= ideal * k_TurnInReachShare)
	{
		locomotion.TurnToFace(creature, FacingToReach(transform, *action.reach, at->position));
		return;
	}
	using MoveResult = CreatureLocomotionSystemInterface::MoveResult;
	const auto result = locomotion.MoveTo(creature, glm::xz(at->position), CreatureLocomotionSystemInterface::Pace::Walk,
	                                      ideal * k_ApproachNear, ideal * k_ApproachFar);
	if (result != MoveResult::Started)
	{
		Fail(action, "navigation failed");
	}
}
} // namespace

bool CreatureObjectActionSystem::Start(entt::entity creature, CreatureObjectAction action)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, CreatureAnimation, Transform>(creature))
	{
		return false;
	}
	const auto& body = registry.Get<const Creature>(creature);
	const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
	action.status = Status::Running;
	action.phase = Phase::Approach;
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
		else if (action.kind == Kind::PickUp && (!CanPickUp(*action.target) || registry.AllOf<HeldByCreature>(*action.target)))
		{
			Fail(action, "it can't be picked up");
		}
		else if (action.kind == Kind::Destroy && !CanDestroy(*action.target))
		{
			Fail(action, "it can't be knocked down");
		}
	}
	if (action.status == Status::Running && action.kind == Kind::Throw)
	{
		const auto& transform = registry.Get<const Transform>(creature);
		const auto ground = glm::distance(glm::xz(transform.position), glm::xz(action.point));
		if (!creature_throw::FarEnoughToThrow(ground, body.size))
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
		bool now = true;
		switch (action.kind)
		{
		case Kind::PutDown:
			single(creature_throw::k_PutDown, points->putDownMs);
			break;
		case Kind::Discard:
			single(creature_throw::k_Discard, points->discardMs);
			break;
		case Kind::Lob:
		{
			// The game times the gentle lob by nothing of its own: it lets go as far through as tossing away does
			const auto discard = DurationOf(creature, creature_throw::k_Discard);
			const auto lob = DurationOf(creature, creature_throw::k_GentleLob);
			single(creature_throw::k_GentleLob, discard > 0.0f ? points->discardMs * lob / discard : points->discardMs);
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
		}
		if (now)
		{
			BeginPlaying(creature, action, registry.Get<CreatureAnimation>(creature));
		}
	}
	const bool started = action.status == Status::Running;
	registry.AssignOrReplaceState<CreatureObjectAction>(creature, std::move(action));
	return started;
}

bool CreatureObjectActionSystem::PickUp(entt::entity creature, entt::entity object)
{
	return Start(creature, {.kind = Kind::PickUp, .target = object});
}

bool CreatureObjectActionSystem::PutDown(entt::entity creature)
{
	return Start(creature, {.kind = Kind::PutDown});
}

bool CreatureObjectActionSystem::Discard(entt::entity creature)
{
	return Start(creature, {.kind = Kind::Discard});
}

bool CreatureObjectActionSystem::Lob(entt::entity creature)
{
	return Start(creature, {.kind = Kind::Lob});
}

bool CreatureObjectActionSystem::EatHeld(entt::entity creature)
{
	const auto held = GetHeld(creature);
	if (held.has_value() && !FoodValueOf(*held).has_value())
	{
		auto action = CreatureObjectAction {.kind = Kind::Eat};
		Fail(action, "what it holds can't be eaten");
		Locator::entitiesRegistry::value().AssignOrReplaceState<CreatureObjectAction>(creature, std::move(action));
		return false;
	}
	return Start(creature, {.kind = Kind::Eat, .target = held});
}

bool CreatureObjectActionSystem::Keep(entt::entity creature, size_t animation)
{
	CreatureObjectAction action {.kind = Kind::Keep};
	action.animations.front() = animation;
	return Start(creature, std::move(action));
}

bool CreatureObjectActionSystem::Throw(entt::entity creature, const glm::vec3& target)
{
	return Start(creature, {.kind = Kind::Throw, .point = target});
}

bool CreatureObjectActionSystem::Destroy(entt::entity creature, entt::entity target)
{
	return Start(creature, {.kind = Kind::Destroy, .target = target});
}

bool CreatureObjectActionSystem::PointAt(entt::entity creature, const glm::vec3& point)
{
	return Start(creature, {.kind = Kind::Point, .point = point});
}

void CreatureObjectActionSystem::Cancel(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(creature) && std::as_const(registry).AllOf<CreatureObjectAction>(creature))
	{
		registry.RemoveState<CreatureObjectAction>(creature);
		if (auto* animation = registry.TryGet<CreatureAnimation>(creature))
		{
			animation->slots.clear();
		}
	}
}

void CreatureObjectActionSystem::Drop(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto held = GetHeld(creature);
	if (!held.has_value())
	{
		return;
	}
	const auto* at = registry.TryGet<const Transform>(*held);
	Release(creature, at != nullptr ? at->position : registry.Get<const Transform>(creature).position, glm::vec3(0.0f));
}

void CreatureObjectActionSystem::Release(entt::entity creature, const glm::vec3& position, const glm::vec3& velocity,
                                         bool atTarget)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
	if (held == nullptr)
	{
		return;
	}
	const auto object = held->object;
	registry.RemoveState<CreatureHeldObject>(creature);
	ReleaseHeld(object, position, velocity, creature, atTarget);
}

void CreatureObjectActionSystem::ReleaseHeld(entt::entity object, const glm::vec3& position, const glm::vec3& velocity,
                                             entt::entity creature, bool atTarget)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return;
	}
	const auto& lookup = std::as_const(registry);
	if (lookup.AllOf<HeldByCreature>(object))
	{
		registry.RemoveState<HeldByCreature>(object);
	}
	if (lookup.AllOf<PhysicsDrawPose>(object))
	{
		registry.Remove<PhysicsDrawPose>(object);
	}
	auto* transform = registry.TryGet<Transform>(object);
	if (transform != nullptr)
	{
		transform->position = position;
	}
	ecs::NotifyTeleported(object);
	// It flies from where it was let go of; the physics puts it back on the map as it comes to rest
	bool placed = false;
	if (transform != nullptr)
	{
		switch (ecs::creature_physics::ReleaseOf(atTarget, creature))
		{
		case ecs::creature_physics::Release::AtTarget:
			placed = ecs::physics::PhysicsObjects::AddObject(object, velocity, glm::vec3(0.0f), creature, false) != nullptr;
			break;
		case ecs::creature_physics::Release::LetGo:
			placed =
			    ecs::physics::from_hand::InitialisePhysicsFromHand(object, velocity, false, std::nullopt, true).has_value();
			break;
		}
		if (!placed)
		{
			// No body could be made for it: it is put down where it is
			ecs::physics::from_hand::PlaceWithoutBody(object);
			placed = true;
		}
	}
	if (!placed)
	{
		ecs::map_cells::InsertMapObject(object);
	}
	// Out of the hand
	ecs::living::InterfaceSetOutMagicHand(object);
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

std::optional<float> CreatureObjectActionSystem::GetProgress(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	if (action == nullptr || action->phase != Phase::Playing ||
	    (action->status != Status::Running && action->status != Status::Contact))
	{
		return std::nullopt;
	}
	if (action->holdMs > 0.0f || action->durationMs <= 0.0f)
	{
		return 0.0f;
	}
	return std::clamp(action->timeMs / action->durationMs, 0.0f, 1.0f);
}

std::optional<entt::entity> CreatureObjectActionSystem::GetHeld(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* held = registry.Valid(creature) ? registry.TryGet<const CreatureHeldObject>(creature) : nullptr;
	return held != nullptr && registry.Valid(held->object) ? std::optional(held->object) : std::nullopt;
}

std::optional<float> CreatureObjectActionSystem::FoodValueOf(entt::entity object) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::infoConstants::has_value() || !ecs::IsAvailable(object))
	{
		return std::nullopt;
	}
	const auto& info = Locator::infoConstants::value();
	float value = 0.0f;
	if (registry.AllOf<Villager>(object))
	{
		const auto* villager = ecs::VillagerInfoOf(object);
		value = villager != nullptr ? villager->foodValue : 0.0f;
	}
	else if (const auto* animal = registry.TryGet<const Animal>(object))
	{
		const auto kind = static_cast<size_t>(animal->type);
		value = kind < info.animal.size() ? info.animal.at(kind).foodValue : 0.0f;
	}
	else if (const auto* mobile = registry.TryGet<const MobileObject>(object))
	{
		const auto kind = static_cast<size_t>(mobile->type);
		value = kind < info.mobileObject.size() ? info.mobileObject.at(kind).foodValue : 0.0f;
	}
	else if (const auto* pot = registry.TryGet<const Pot>(object))
	{
		// A pot or pile of food is worth as much as the food in it. A storage pit's pile is eaten from where it lies,
		// which isn't done here.
		const auto kind = static_cast<size_t>(pot->type);
		const bool food = kind < info.pot.size() && info.pot.at(kind).resourceType == ResourceType::Food &&
		                  pot->type != PotInfo::StoragePitFoodPile;
		value = food ? static_cast<float>(pot->amount) : 0.0f;
	}
	return value > 0.0f ? std::optional(value) : std::nullopt;
}

bool CreatureObjectActionSystem::CanPickUp(entt::entity object) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(object) || !registry.AllOf<Transform>(object))
	{
		return false;
	}
	// An animal only when it may be put in a hand
	if (registry.AllOf<Animal>(object))
	{
		return ecs::animal_ai::ValidForPlaceInHand(object);
	}
	// Of pots and piles, only food can be picked up, to eat
	return registry.AnyOf<MobileObject, Villager>(object) || (registry.AllOf<Pot>(object) && FoodValueOf(object).has_value());
}

bool CreatureObjectActionSystem::CanDestroy(entt::entity target) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	return ecs::IsAvailable(target) && registry.AllOf<Transform>(target) && registry.AnyOf<Tree, Abode, MobileObject>(target);
}

void CreatureObjectActionSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!Locator::creatureLocomotionSystem::has_value() || !Locator::creatureAnimationSystem::has_value())
	{
		return;
	}

	// What is held by a creature that is gone, or no longer holds it, falls where it is
	std::vector<entt::entity> loose;
	lookup.Each<const HeldByCreature>([&](entt::entity entity, const HeldByCreature& by) {
		const auto* held = lookup.Valid(by.creature) ? lookup.TryGet<const CreatureHeldObject>(by.creature) : nullptr;
		if (held == nullptr || held->object != entity)
		{
			loose.push_back(entity);
		}
	});
	for (const auto entity : loose)
	{
		const auto* at = lookup.TryGet<const Transform>(entity);
		ReleaseHeld(entity, at != nullptr ? at->position : glm::vec3(0.0f), glm::vec3(0.0f));
	}
	std::vector<entt::entity> emptyHanded;
	lookup.Each<const CreatureHeldObject>([&](entt::entity entity, const CreatureHeldObject& held) {
		if (!ecs::IsAvailable(held.object))
		{
			emptyHanded.push_back(entity);
		}
	});
	for (const auto entity : emptyHanded)
	{
		registry.RemoveState<CreatureHeldObject>(entity);
	}

	std::vector<entt::entity> acting;
	lookup.Each<const Creature, const Transform, const CreatureAnimation, const CreatureObjectAction>(
	    [&acting](entt::entity entity, const auto&...) { acting.push_back(entity); });

	// Walking or turning up to what they act on
	for (const auto entity : acting)
	{
		auto& action = registry.Get<CreatureObjectAction>(entity);
		if (action.status == Status::Running && action.phase == Phase::Approach)
		{
			Approach(entity, action, lookup.Get<Creature>(entity), lookup.Get<Transform>(entity),
			         registry.Get<CreatureAnimation>(entity), lookup.TryGet<const CreatureHeldObject>(entity));
		}
	}

	// The actions play on by a turn
	for (const auto entity : acting)
	{
		if (!lookup.AllOf<CreatureObjectAction>(entity))
		{
			continue;
		}
		auto& action = registry.Get<CreatureObjectAction>(entity);
		if (action.phase != Phase::Playing || (action.status != Status::Running && action.status != Status::Contact))
		{
			continue;
		}
		const auto& body = lookup.Get<Creature>(entity);
		const auto& transform = lookup.Get<Transform>(entity);
		action.timeMs += k_TurnMs * creature_layers::PlaybackRate(body.size);

		// Until it has hold, the reach follows what it reaches for, and fails when it gets out of reach
		if ((action.kind == Kind::PickUp || action.kind == Kind::Destroy) && !action.eventDone && action.reach.has_value())
		{
			const auto* at = action.target.has_value() && ecs::IsAvailable(*action.target)
			                     ? lookup.TryGet<const Transform>(*action.target)
			                     : nullptr;
			if (at == nullptr)
			{
				Fail(action, "it has gone");
				continue;
			}
			const auto local = ToLocal(transform, at->position);
			const auto reach =
			    creature_reach::Solve(action.mirrored ? creature_reach::Mirrored(*action.reach) : *action.reach, local);
			if (!reach.inRange)
			{
				Fail(action, "it got out of reach");
				continue;
			}
			action.weights = reach.weights;
		}

		if (action.holdMs > 0.0f)
		{
			action.holdMs -= k_TurnMs;
			if (action.holdMs <= 0.0f)
			{
				action.status = Status::Done;
			}
		}
		else if (action.timeMs >= action.durationMs && (action.eventDone || action.eventMs > action.durationMs))
		{
			action.status = Status::Done;
		}
	}

	// The moments things are taken hold of, knocked down, let go of and eaten
	for (const auto entity : acting)
	{
		const auto* action = lookup.TryGet<const CreatureObjectAction>(entity);
		if (action != nullptr && action->phase == Phase::Playing && action->status == Status::Running && !action->eventDone &&
		    action->timeMs >= action->eventMs)
		{
			Moment(entity);
		}
	}

	// What is held rides in the hand, turned with it; out of the map cells while it is held
	lookup.Each<const Creature, const Transform, const CreatureAnimation, const CreatureHeldObject>(
	    [&](const Creature&, const Transform& transform, const CreatureAnimation& animation, const CreatureHeldObject& held) {
		    if (!ecs::IsAvailable(held.object) || !lookup.AllOf<Transform>(held.object) || animation.boneMatrices.empty())
		    {
			    return;
		    }
		    auto& at = registry.Get<Transform>(held.object);
		    // Held by its middle in the palm, turned with the creature
		    at.rotation = transform.rotation * held.rotation;
		    at.position = PalmOf(held.bone, animation, PlacementOf(transform)) - (at.rotation * held.middle);
	    });

	// Carrying something heavy makes it stronger as it walks about
	std::vector<entt::entity> bodies;
	lookup.Each<const Creature, const CreatureNeeds>(
	    [&bodies](entt::entity entity, const auto&...) { bodies.push_back(entity); });
	for (const auto entity : bodies)
	{
		auto& needs = registry.Get<CreatureNeeds>(entity);
		needs.carriedWeight.reset();
		const auto held = GetHeld(entity);
		if (!held.has_value())
		{
			continue;
		}
		const auto weight = WeightOf(lookup, *held);
		const auto own = WeightOf(lookup, entity);
		if (weight.has_value() && own.has_value() && *own > 0.0f)
		{
			needs.carriedWeight = std::clamp(*weight / *own, 0.0f, 1.0f);
		}
	}

	// The town sees what the creature does with fear or respect, and keeps that view a while after
	std::vector<entt::entity> creatures;
	lookup.Each<const Creature>([&creatures](entt::entity entity, const Creature&) { creatures.push_back(entity); });
	for (const auto entity : creatures)
	{
		const auto* action = lookup.TryGet<const CreatureObjectAction>(entity);
		const bool busy = action != nullptr && (action->status == Status::Running || action->status == Status::Contact);
		const auto held = GetHeld(entity);
		const bool holdingVillager = IsVillager(lookup, held);
		auto attitude = creature_object_actions::TownAttitude::None;
		if (busy)
		{
			attitude = creature_object_actions::AttitudeTo(action->kind, IsVillager(lookup, action->target), holdingVillager);
		}
		else if (holdingVillager)
		{
			attitude = creature_object_actions::TownAttitude::Fear;
		}
		if (attitude != creature_object_actions::TownAttitude::None)
		{
			registry.AssignOrReplaceState<CreatureTownAttitude>(
			    entity,
			    CreatureTownAttitude {.attitude = attitude, .secondsLeft = creature_object_actions::AttitudeSeconds(attitude)});
		}
		else if (lookup.AllOf<CreatureTownAttitude>(entity))
		{
			auto& seen = registry.Get<CreatureTownAttitude>(entity);
			if (seen.attitude != creature_object_actions::TownAttitude::None)
			{
				seen.secondsLeft -= k_TurnSeconds;
				if (seen.secondsLeft <= 0.0f)
				{
					seen = {};
				}
			}
		}
	}
}

void CreatureObjectActionSystem::Moment(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	auto& action = registry.Get<CreatureObjectAction>(creature);
	const auto& body = lookup.Get<Creature>(creature);
	const auto& transform = lookup.Get<Transform>(creature);
	const auto& animation = lookup.Get<CreatureAnimation>(creature);
	const auto* points = PointsOf(body);
	action.eventDone = true;
	if (points == nullptr)
	{
		return;
	}
	// The palm as the animations have it at the moment, else as the body was last drawn
	const auto handPosition =
	    PalmAtMoment(creature, action, *points, animation, transform)
	        .value_or(animation.boneMatrices.empty()
	                      ? transform.position
	                      : PalmOf(HandBone(*points, animation, action.mirrored), animation, PlacementOf(transform)));
	switch (action.kind)
	{
	case Kind::PickUp:
	{
		const auto object = *action.target;
		if (!ecs::IsAvailable(object) || lookup.AllOf<HeldByCreature>(object) || !lookup.AllOf<Transform>(object))
		{
			Fail(action, "it has gone");
			break;
		}
		const auto bone = HandBone(*points, animation, action.mirrored);
		const auto middle = MiddleOf(registry, object);
		const auto rotation = glm::transpose(transform.rotation) * lookup.Get<Transform>(object).rotation;
		const bool mirrored = action.mirrored;
		action.status = Status::Contact;
		// Taken out of the physics, into the hand and off the map, as the player's hand takes things
		ecs::physics::PhysicsObjects::RemoveObject(object);
		ecs::living::InterfaceSetInMagicHand(object);
		if (lookup.AllOf<Pot>(object))
		{
			ecs::animal_ai::RemovePotReaction(object);
		}
		if (ecs::map_cells::IsObjectInMap(object))
		{
			ecs::map_cells::RemoveMapObject(object);
		}
		if (lookup.AllOf<MorphWithTerrain>(object))
		{
			registry.Remove<MorphWithTerrain>(object);
		}
		registry.AssignOrReplaceState<CreatureHeldObject>(
		    creature,
		    CreatureHeldObject {.object = object, .mirrored = mirrored, .bone = bone, .rotation = rotation, .middle = middle});
		registry.AssignOrReplaceState<HeldByCreature>(object, HeldByCreature {.creature = creature});
		// Into the palm, by its middle, drawn there at once
		auto& at = registry.Get<Transform>(object);
		at.rotation = transform.rotation * rotation;
		at.position = handPosition - (at.rotation * middle);
		ecs::NotifyTeleported(object);
		break;
	}
	case Kind::Destroy:
	{
		const auto target = *action.target;
		action.status = Status::Contact;
		if (!ecs::IsAvailable(target))
		{
			break;
		}
		// (approximate) homes take a blow as a thrown thing gives one; trees are felled; things are knocked to pieces
		if (lookup.AllOf<Abode>(target))
		{
			ecs::abodes::OnPhysicalDamage(
			    target, {.remaining = std::nullopt,
			             .hitter = creature,
			             .player = body.owner != PlayerNames::NEUTRAL ? std::optional(body.owner) : std::nullopt,
			             .byCreature = true});
		}
		else if (lookup.AllOf<Tree>(target))
		{
			ecs::FellTree(target, creature);
		}
		else
		{
			ecs::ToBeDeleted(target);
		}
		break;
	}
	case Kind::PutDown:
		Release(creature, handPosition, glm::vec3(0.0f));
		break;
	case Kind::Discard:
	case Kind::Lob:
	{
		const auto animationIndex = action.animations.front();
		const auto now = HandAt(creature, transform, *points, animationIndex, action.eventMs);
		const auto before =
		    HandAt(creature, transform, *points, animationIndex, action.eventMs - creature_throw::k_HandSpeedSpanMs);
		const auto velocity =
		    now.has_value() && before.has_value()
		        ? creature_throw::TossVelocity(creature_throw::HandVelocity(*now, *before, creature_throw::k_HandSpeedSpanMs),
		                                       action.mirrored, transform.rotation, creature_throw::k_DiscardSpeedShare)
		        : glm::vec3(0.0f);
		Release(creature, handPosition, velocity);
		break;
	}
	case Kind::Throw:
		Release(creature, handPosition,
		        creature_throw::ReleaseVelocity(action.point, handPosition, std::max(action.flightSeconds, k_Tiny)), true);
		break;
	case Kind::Eat:
	{
		const auto held = GetHeld(creature);
		if (!held.has_value())
		{
			break;
		}
		const auto value = FoodValueOf(*held);
		registry.RemoveState<CreatureHeldObject>(creature);
		if (lookup.AllOf<HeldByCreature>(*held))
		{
			registry.RemoveState<HeldByCreature>(*held);
		}
		Consume(lookup, *held);
		if (value.has_value() && Locator::creaturePhysiologySystem::has_value())
		{
			Locator::creaturePhysiologySystem::value().Eat(creature, *value);
		}
		break;
	}
	case Kind::Keep:
	case Kind::Point:
		break;
	}
}

void CreatureObjectActionSystem::UpdateDraw(float turnFraction)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	const auto f = std::clamp(turnFraction, 0.0f, 1.0f);

	// The actions' animations, a share of the way through the turn on
	std::vector<entt::entity> acting;
	lookup.Each<const Creature, const CreatureAnimation, const CreatureObjectAction>(
	    [&acting](entt::entity entity, const auto&...) { acting.push_back(entity); });
	for (const auto entity : acting)
	{
		const auto& action = lookup.Get<CreatureObjectAction>(entity);
		auto& animation = registry.Get<CreatureAnimation>(entity);
		if (action.phase != Phase::Playing || (action.status != Status::Running && action.status != Status::Contact))
		{
			continue;
		}
		animation.slots.clear();
		const auto drawnMs = action.timeMs + (f * k_TurnMs * creature_layers::PlaybackRate(lookup.Get<Creature>(entity).size));
		for (size_t i = 0; i < action.animationCount; ++i)
		{
			const auto duration = DurationOf(entity, action.animations.at(i));
			// Pointing loops; everything else holds its last frame
			const auto time = action.holdMs > 0.0f && duration > 0.0f
			                      ? std::fmod(drawnMs, duration)
			                      : std::clamp(drawnMs, 0.0f, std::max(duration - 1.0f, 0.0f));
			animation.slots.push_back({.animation = action.animations.at(i),
			                           .timeMs = time,
			                           .weight = action.weights.at(i),
			                           .mirrored = action.mirrored});
		}
	}
}

void CreatureObjectActionSystem::UpdateHeldDraw()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);

	// What is held is drawn in the hand as the body is drawn this frame
	bool moved = false;
	std::vector<entt::entity> holding;
	lookup.Each<const Creature, const Transform, const CreatureAnimation, const CreatureHeldObject>(
	    [&holding](entt::entity entity, const auto&...) { holding.push_back(entity); });
	for (const auto entity : holding)
	{
		const auto& held = lookup.Get<CreatureHeldObject>(entity);
		const auto& animation = lookup.Get<CreatureAnimation>(entity);
		if (!ecs::IsAvailable(held.object) || !lookup.AllOf<Transform>(held.object) || animation.boneMatrices.empty())
		{
			continue;
		}
		// in the hand as the body is drawn this frame (its own drawn matrix: between its turns), turned with the body;
		// the object's Transform stays where the turn put it
		const auto rotation = ecs::creature_pose::DrawnPlacementOf(lookup, entity).rotation * held.rotation;
		const auto position = PalmOf(held.bone, animation, ecs::DrawnBodyModel(lookup, entity)) - (rotation * held.middle);
		auto& pose = lookup.AllOf<PhysicsDrawPose>(held.object) ? registry.Get<PhysicsDrawPose>(held.object)
		                                                        : registry.Assign<PhysicsDrawPose>(held.object);
		if (pose.position != position || pose.rotation != rotation)
		{
			pose.position = position;
			pose.rotation = rotation;
			moved = true;
		}
	}
	if (moved)
	{
		registry.SetDirty();
	}
}
