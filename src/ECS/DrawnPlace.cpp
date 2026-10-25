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
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Shark.h"
#include "ECS/Registry.h"

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
	if (const auto* animation = registry.TryGet<const components::CreatureAnimation>(entity); animation != nullptr)
	{
		return components::DrawnTransform(transform, animation);
	}
	if (const auto* holder = registry.TryGet<const components::HeldByCreature>(entity);
	    holder != nullptr && holder->drawn.has_value())
	{
		return {.position = holder->drawn->position, .rotation = holder->drawn->rotation, .scale = transform.scale};
	}
	if (const auto* carried = registry.TryGet<const components::CarriedByTornado>(entity);
	    carried != nullptr && carried->drawn.has_value())
	{
		return {.position = carried->drawn->position, .rotation = carried->drawn->rotation, .scale = transform.scale};
	}
	if (const auto* pulled = registry.TryGet<const components::HandPulled>(entity);
	    pulled != nullptr && drawn_place::StandsAsPulled(*pulled, transform))
	{
		return {.position = pulled->position, .rotation = pulled->rotation, .scale = pulled->scale};
	}
	if (const auto* inHand = registry.TryGet<const components::InHand>(entity); inHand != nullptr && inHand->drawn.has_value())
	{
		return {.position = inHand->drawn->position, .rotation = inHand->drawn->rotation, .scale = transform.scale};
	}
	if (const auto* animal = registry.TryGet<const components::Animal>(entity); animal != nullptr && animal->drawn.has_value())
	{
		return {.position = animal->drawn->position, .rotation = animal->drawn->rotation, .scale = transform.scale};
	}
	if (const auto* shark = registry.TryGet<const components::Shark>(entity); shark != nullptr && shark->drawn.has_value())
	{
		return {.position = shark->drawn->position, .rotation = shark->drawn->rotation, .scale = transform.scale};
	}
	return transform;
}

} // namespace openblack::ecs
