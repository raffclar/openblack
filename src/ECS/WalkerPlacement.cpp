/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WalkerPlacement.h"

#include <glm/gtx/vec_swizzle.hpp>

#include "ECS/Components/Transform.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/WallHugRules.h"

using namespace openblack::ecs;
using namespace openblack::ecs::components;

void walker_placement::Place(Registry& registry, entt::entity entity, glm::vec3 position)
{
	auto* transform = registry.TryGet<Transform>(entity);
	if (transform == nullptr)
	{
		return;
	}
	transform->position = position;
	auto* wallHug = registry.TryGet<WallHug>(entity);
	if (wallHug == nullptr)
	{
		return;
	}
	const auto metres = glm::xz(position);
	wallHug->position = wall_hug::ToWhole(metres);
	wallHug->placedAt = metres;
	const bool onItsWay = registry.AnyOf<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                                     MoveStateFinalStepTag>(entity);
	const bool arrived = registry.AllOf<MoveStateArrivedTag>(entity);
	if (!onItsWay && !arrived)
	{
		// Not walking: nothing of a walk to take up
		return;
	}
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(entity);
	registry.Remove<WallHugObjectReference>(entity);
	wallHug->step = {0, 0};
	wallHug->turnsUntilStepRebuild = 0;
	if (arrived)
	{
		// Standing at its goal, it stands where it is put rather than walking back
		wallHug->goal = metres;
		registry.Assign<MoveStateArrivedTag>(entity, MoveStateClockwise::Undefined, metres);
		return;
	}
	// On its way, it sets off for its goal from here, as a walk newly set up does
	registry.Assign<MoveStateLinearTag>(entity, MoveStateClockwise::Undefined, metres);
}

void walker_placement::Stop(Registry& registry, entt::entity entity)
{
	auto* wallHug = registry.TryGet<WallHug>(entity);
	if (wallHug == nullptr)
	{
		return;
	}
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(entity);
	registry.Remove<WallHugObjectReference>(entity);
	wallHug->step = {0, 0};
	wallHug->turnsUntilStepRebuild = 0;
}
