/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DrawnPlace.h"

#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Registry.h"

namespace openblack::ecs
{

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
	return transform;
}

} // namespace openblack::ecs
