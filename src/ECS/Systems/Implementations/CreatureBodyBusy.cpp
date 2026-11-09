/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureBodyBusy.h"

#include "Creature/CreatureLayers.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

creature_body::Activity creature_body_busy::ActivityOf(const ecs::Registry& registry, entt::entity creature,
                                                       const CreatureLocomotionSystemInterface* locomotion,
                                                       const CreatureObjectActionSystemInterface* hands)
{
	const auto* animation = registry.Valid(creature) ? registry.TryGet<const CreatureAnimation>(creature) : nullptr;
	return {
	    .bodyPlays = animation != nullptr && creature_layers::IsPlaying(animation->body),
	    .objectActs = hands != nullptr && hands->BodyActs(creature),
	    .moving = locomotion != nullptr && locomotion->IsMoving(creature),
	};
}

creature_body::Activity creature_body_busy::ActivityOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* locomotion =
	    Locator::creatureLocomotionSystem::has_value() ? &Locator::creatureLocomotionSystem::value() : nullptr;
	const auto* hands =
	    Locator::creatureObjectActionSystem::has_value() ? &Locator::creatureObjectActionSystem::value() : nullptr;
	return ActivityOf(registry, creature, locomotion, hands);
}

bool creature_body_busy::PerformsBodyAction(const ecs::Registry& registry, entt::entity creature)
{
	return creature_body::PerformsBodyAction(ActivityOf(registry, creature));
}
