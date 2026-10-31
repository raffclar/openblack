/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DrawnPlace.h"

#include "ECS/Components/Animal.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/DrawnHeight.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Registry.h"
#include "Nature/Fireflies.h"

namespace openblack::ecs
{

bool drawn_place::StandsAsPulled(const components::HandPulled& pulled, const components::Transform& transform)
{
	return pulled.standsAt == transform.position && pulled.standsRotation == transform.rotation &&
	       pulled.standsScale == transform.scale;
}

components::Transform drawn_place::DrawnTransformOf(const Registry& registry, entt::entity entity,
                                                    const components::Transform& transform)
{
	return DrawnPlaces(registry).Of(entity, transform);
}

drawn_place::DrawnPlaces::DrawnPlaces(const Registry& registry)
    : _animation(registry)
    , _holder(registry)
    , _carried(registry)
    , _pulled(registry)
    , _inHand(registry)
    , _animal(registry)
    , _shark(registry)
    , _chest(registry)
    , _flag(registry)
    , _firefly(registry)
    , _height(registry)
{
}

components::Transform drawn_place::DrawnPlaces::Of(entt::entity entity, const components::Transform& transform) const
{
	if (const auto* animation = _animation.TryGet(entity); animation != nullptr)
	{
		return components::DrawnTransform(transform, animation);
	}
	if (const auto* holder = _holder.TryGet(entity); holder != nullptr && holder->drawn.has_value())
	{
		return {.position = holder->drawn->position, .rotation = holder->drawn->rotation, .scale = transform.scale};
	}
	if (const auto* carried = _carried.TryGet(entity); carried != nullptr && carried->drawn.has_value())
	{
		return {.position = carried->drawn->position, .rotation = carried->drawn->rotation, .scale = transform.scale};
	}
	if (const auto* pulled = _pulled.TryGet(entity); pulled != nullptr && drawn_place::StandsAsPulled(*pulled, transform))
	{
		return {.position = pulled->position, .rotation = pulled->rotation, .scale = pulled->scale};
	}
	if (const auto* inHand = _inHand.TryGet(entity); inHand != nullptr && inHand->drawn.has_value())
	{
		return {.position = inHand->drawn->position, .rotation = inHand->drawn->rotation, .scale = transform.scale};
	}
	if (const auto* animal = _animal.TryGet(entity); animal != nullptr && animal->drawn.has_value())
	{
		return {.position = animal->drawn->position, .rotation = animal->drawn->rotation, .scale = transform.scale};
	}
	if (const auto* shark = _shark.TryGet(entity); shark != nullptr && shark->drawn.has_value())
	{
		return {.position = shark->drawn->position, .rotation = shark->drawn->rotation, .scale = transform.scale};
	}
	// A reward chest that falls from the sky is drawn between the places of its last two turns
	if (const auto* chest = _chest.TryGet(entity); chest != nullptr && chest->drawn.has_value())
	{
		return {.position = chest->drawn->position, .rotation = chest->drawn->rotation, .scale = transform.scale};
	}
	// A town's flag that shows turns to the camera, at the height and size of its level, where the frames last drew it
	if (const auto* flag = _flag.TryGet(entity); flag != nullptr && flag->shown)
	{
		return {.position = flag->drawnPosition, .rotation = flag->drawnRotation, .scale = glm::vec3(flag->drawnScale)};
	}
	// A firefly out of hiding drifts about its place, where the frames last drew it, at the size of its sprite
	if (const auto* firefly = _firefly.TryGet(entity); firefly != nullptr)
	{
		return {.position = firefly->drawn,
		        .rotation = transform.rotation,
		        .scale = glm::vec3(fireflies::k_SpriteHalfSize, fireflies::k_SpriteHalfSize, 1.0f)};
	}
	// A town's totem and its icon rise and sink by the computer's clock, at the height where the frames last put them
	if (const auto* height = _height.TryGet(entity); height != nullptr)
	{
		return {.position = {transform.position.x, height->y, transform.position.z},
		        .rotation = transform.rotation,
		        .scale = transform.scale};
	}
	return transform;
}

} // namespace openblack::ecs
