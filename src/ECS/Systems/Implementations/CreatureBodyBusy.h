/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "Creature/CreatureBodyActivity.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::systems
{
class CreatureLocomotionSystemInterface;
class CreatureObjectActionSystemInterface;
} // namespace openblack::ecs::systems

/// What the body of a creature does now, read from its animation, its legs and its hands (see creature_body). Each
/// system that asks whether the body of a creature is busy uses these.
namespace openblack::ecs::systems::creature_body_busy
{

/// What the body of the creature does now. The legs and the hands that are given are read. With no legs or no hands,
/// that part does nothing.
[[nodiscard]] creature_body::Activity ActivityOf(const ecs::Registry& registry, entt::entity creature,
                                                 const CreatureLocomotionSystemInterface* locomotion,
                                                 const CreatureObjectActionSystemInterface* hands);
/// What the body of the creature does now, with the legs and the hands of the locator
[[nodiscard]] creature_body::Activity ActivityOf(const ecs::Registry& registry, entt::entity creature);
/// Whether the body of the creature does an action, a move included, with the legs and the hands of the locator
[[nodiscard]] bool PerformsBodyAction(const ecs::Registry& registry, entt::entity creature);

} // namespace openblack::ecs::systems::creature_body_busy
