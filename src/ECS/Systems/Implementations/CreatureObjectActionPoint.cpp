/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <span>
#include <vector>

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "Creature/CreatureObjectActions.h"
#include "Creature/CreaturePointing.h"
#include "Creature/CreatureRig.h"
#include "CreatureObjectActionSystem.h"
#include "CreatureObjectActionSystemDetail.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureCarried.h"
#include "ECS/CreaturePlacement.h"
#include "ECS/Map.h"
#include "ECS/PhysicsClasses.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"
#include "ObjectMeasures.h"
#include "Sound/SoundRequestInterface.h"
#include "VillagerPhysics.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature_object_actions::Kind;
using openblack::creature_object_actions::Status;
using Phase = openblack::ecs::components::CreatureObjectAction::Phase;
using namespace openblack::ecs::systems::object_action_detail;

namespace openblack::ecs::systems::object_action_detail
{
void SetPointing(CreatureObjectAction& action, const Transform& transform, const Creature& body, const CreatureHeldObject* held,
                 bool choosesArm)
{
	const auto origin = transform.position + glm::vec3(0.0f, creature_pointing::k_OriginHeightPerSize * ShownSize(body), 0.0f);
	const auto facing = glm::xz(transform.rotation * (-k_MeshBack));
	const auto aim = creature_pointing::AimAt(origin, facing, action.point);
	if (choosesArm && action.pointTurned)
	{
		action.mirrored =
		    creature_pointing::PointsWithLeftAfterTurn(aim, held != nullptr ? std::optional(held->mirrored) : std::nullopt);
	}
	const auto weights = creature_pointing::Weights(aim, action.mirrored);
	SetSlots(action, creature_pointing::k_Animations, weights);
}
} // namespace openblack::ecs::systems::object_action_detail

bool CreatureObjectActionSystem::PointAt(entt::entity creature, const glm::vec3& point)
{
	return Start(creature, {.kind = Kind::Point, .point = point});
}

bool CreatureObjectActionSystem::PointAtFor(entt::entity creature, const glm::vec3& point, float seconds,
                                            std::optional<entt::entity> thing)
{
	return Start(creature, {.kind = Kind::Point, .target = thing, .point = point, .pointSeconds = seconds});
}

bool CreatureObjectActionSystem::PointUntilTold(entt::entity creature, const glm::vec3& point)
{
	return Start(creature, {.kind = Kind::Point, .point = point, .pointsUntilTold = true});
}

void CreatureObjectActionSystem::MovePoint(entt::entity creature, const glm::vec3& point)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* action = registry.Valid(creature) ? registry.TryGet<CreatureObjectAction>(creature) : nullptr;
	if (action != nullptr && action->kind == Kind::Point)
	{
		action->point = point;
	}
}

void CreatureObjectActionSystem::EndPoint(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* action = registry.Valid(creature) ? registry.TryGet<CreatureObjectAction>(creature) : nullptr;
	if (action == nullptr || action->kind != Kind::Point || action->status != Status::Running)
	{
		return;
	}
	if (action->phase != Phase::Playing && !action->pointTurned)
	{
		// It has not started to point yet: it stops at once
		action->status = Status::Done;
		return;
	}
	// The arm plays to the end of its animations with no hold. A turn to the point ends first, and then the arm plays
	// all of its animations.
	action->holdMs = 0.0f;
	action->pointEnding = true;
}

std::optional<glm::vec3> CreatureObjectActionSystem::GetPointingHand(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return std::nullopt;
	}
	const auto* action = registry.TryGet<const CreatureObjectAction>(creature);
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (action == nullptr || animation == nullptr || transform == nullptr || action->kind != Kind::Point ||
	    action->phase != Phase::Playing || action->status != Status::Running || action->pointEnding)
	{
		return std::nullopt;
	}
	const auto* points = PointsOf(registry.Get<const Creature>(creature));
	if (points == nullptr || animation->boneMatrices.empty())
	{
		return std::nullopt;
	}
	const auto hand = HandBone(*points, *animation, action->mirrored);
	if (hand >= animation->boneMatrices.size())
	{
		return std::nullopt;
	}
	const auto pose = creature::TurnPose(animation->newerTurnPose, animation->boneMatrices, PlacementOf(*transform));
	return glm::vec3(creature::PosedBone(hand, pose.bones, pose.placement)[3]);
}
