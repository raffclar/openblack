/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerMoveToObject.h"

#include <glm/gtx/vec_swizzle.hpp>

#include "Common/GUtilsDistance.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillagerWork.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/VillagerWorkRules.h"
#include "ECS/WallHugRules.h"
#include "Locator.h"
#include "ObjectMeasures.h"
#include "VillagerHome.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace move_to_object = openblack::ecs::villager_move_to_object;

namespace
{
/// A walker going slower than the thing it walks to goes this much faster than the thing
constexpr float k_FasterThanTarget = 1.25f;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

glm::vec2 GroundOf(entt::entity entity)
{
	return glm::xz(Entities().Get<const Transform>(entity).position);
}

/// How far a thing goes in a turn, in metres
float StepOf(entt::entity thing)
{
	auto& registry = Entities();
	if (const auto* animal = registry.TryGet<const Animal>(thing))
	{
		return gutils::ConvertWholeDistanceToMeters(animal->move.speed);
	}
	if (const auto* walker = registry.TryGet<const WallHug>(thing))
	{
		return wall_hug::StepMetres(walker->speed);
	}
	return 0.0f;
}

bool IsThere(entt::entity thing)
{
	return thing != entt::null && Entities().Valid(thing) && Entities().AllOf<Transform>(thing);
}
} // namespace

bool move_to_object::SetupMoveToObject(LivingAction& action, entt::entity thing, VillagerStates final)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	registry.AssignOrReplace<MoveTarget>(villager, MoveTarget {.thing = thing});
	auto& living = Locator::livingActionSystem::value();
	if (!living.VillagerSetCurrentAndDestinationState(action, VillagerStates::MoveToObject, final))
	{
		return false;
	}
	// With nothing to walk to, it decides again on its next turn
	if (!IsThere(thing))
	{
		return true;
	}
	// It first heads for where it would stand by the thing, stepping straight there; from then on, for the thing itself
	const auto goal = villager_work::EdgeWorkingPosition(GroundOf(thing), GroundOf(villager),
	                                                     systems::object_measures::TwoDRadius(registry, thing),
	                                                     systems::object_measures::TwoDRadius(registry, villager));
	villager_home::SetupMobileMoveTo(action, goal, villager_home::WalkWay::StraightThrough);
	return true;
}

uint32_t move_to_object::MoveToObject(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	auto& living = Locator::livingActionSystem::value();
	const auto* target = registry.TryGet<const MoveTarget>(villager);
	const auto thing = target != nullptr ? target->thing : entt::null;
	// The thing has gone: it decides again
	if (!IsThere(thing))
	{
		registry.Remove<MoveTarget>(villager);
		living.VillagerSetState(action, LivingAction::Index::Top, VillagerStates::DecideWhatToDo, false);
		return 1;
	}
	auto& wallHug = registry.Get<WallHug>(villager);
	const auto there = GroundOf(thing);
	float step = wall_hug::StepMetres(wallHug.speed);
	// Within a step of the thing it has arrived
	if (step > gutils::GetDistanceInMetres(map_coords::FromMetres(GroundOf(villager)), map_coords::FromMetres(there)))
	{
		registry.Remove<MoveTarget, MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
		living.VillagerSetTopStateToFinal(action);
		return 0;
	}
	// It keeps up with a thing faster than itself
	const float targetStep = StepOf(thing) * k_FasterThanTarget;
	if (step < targetStep)
	{
		step = targetStep;
		wallHug.speed = GetSpeedStateSpeed(static_cast<SpeedState>(gutils::ConvertMetersToWholeDistance(step)));
	}
	wallHug.goal = there;
	// Once its walk reaches where the thing was, it has arrived too
	if (villager_home::WalkIsOverAt(villager, wallHug.goal))
	{
		registry.Remove<MoveTarget, MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
		living.VillagerSetTopStateToFinal(action);
	}
	return 0;
}
