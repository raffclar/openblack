/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreaturePose.h"

#include <cmath>

#include <utility>

#include <glm/geometric.hpp>

#include "3D/ObjectMatrix.h"
#include "Creature/CreatureLocomotion.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/PhysicsDrawPose.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// Below this the follower did not move far enough to tell its way
constexpr float k_HardlyMoved = 1e-4f;
} // namespace

void creature_pose::CommitTurnPose(entt::entity creature, glm::vec3 position, float heading)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !std::as_const(registry).AllOf<Transform>(creature))
	{
		return;
	}
	map_cells::MoveMapObject(creature, position);
	registry.Get<Transform>(creature).rotation = affine::AngleY(-heading);
}

float creature_pose::ReadHeading(const glm::mat3& rotation)
{
	return std::atan2(rotation[2].x, rotation[2].z);
}

float creature_pose::HeadingFromFollower(glm::vec2 before, glm::vec2 after, float previous)
{
	const auto way = after - before;
	return glm::length(way) > k_HardlyMoved ? creature_locomotion::HeadingOf(way) : previous;
}

std::optional<creature_pose::DrawnPlacement> creature_pose::BetweenTurns(const Registry& registry, entt::entity creature)
{
	// the drawn pose first: every other entity has none, so the walk over the rows stops there
	const auto* pose = registry.TryGet<const CreatureDrawPose>(creature);
	if (pose == nullptr)
	{
		return std::nullopt;
	}
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* locomotion = registry.TryGet<const CreatureLocomotion>(creature);
	// the Transform is where the locomotion's turn put it (an exact copy, so the compare is exact); moved there by
	// anything else, the pose is not that turn's
	if (transform == nullptr || locomotion == nullptr || !locomotion->started ||
	    locomotion->toPosition != transform->position || registry.AnyOf<HandDrawPose, PhysicsDrawPose>(creature))
	{
		return std::nullopt;
	}
	return DrawnPlacement {.position = pose->position, .rotation = pose->rotation};
}

creature_pose::DrawnPlacement creature_pose::DrawnPlacementOf(const Registry& registry, entt::entity creature)
{
	// the sources of ecs::DrawnModel in its order, so that its rotation is the one the body is drawn with
	if (const auto* inHand = registry.TryGet<const HandDrawPose>(creature))
	{
		return {.position = inHand->position, .rotation = inHand->rotation};
	}
	if (const auto* flying = registry.TryGet<const PhysicsDrawPose>(creature))
	{
		return {.position = flying->position, .rotation = flying->rotation};
	}
	if (const auto between = BetweenTurns(registry, creature))
	{
		return *between;
	}
	const auto& transform = registry.Get<const Transform>(creature);
	return {.position = transform.position, .rotation = transform.rotation};
}

glm::vec3 creature_pose::DrawnScale(const Registry& registry, entt::entity creature)
{
	const auto& transform = registry.Get<const Transform>(creature);
	const auto* pose = registry.TryGet<const CreatureDrawPose>(creature);
	return pose != nullptr && pose->scale.has_value() ? *pose->scale : transform.scale;
}

float creature_pose::DrawnSizeShare(const Registry& registry, entt::entity creature)
{
	const auto* pose = registry.TryGet<const CreatureDrawPose>(creature);
	const auto own = registry.Get<const Transform>(creature).scale.x;
	if (pose == nullptr || !pose->scale.has_value() || !(own > 0.0f))
	{
		return 1.0f;
	}
	return pose->scale->x / own;
}
