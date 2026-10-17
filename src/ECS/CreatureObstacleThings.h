/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "Creature/CreatureObstacles.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::components
{
struct Creature;
}

/// The things on the map and the creatures as the creatures' rule for what they must go round sees them. The routes a
/// creature plans and its search for clear land both use this one rule.
namespace openblack::ecs::creature_obstacle_things
{

/// A building no more built than this on its site is only just begun
inline constexpr float k_JustBegun = 0.01f;

/// What a thing on the map is to a creature that must go round it or not, none for what it never meets
[[nodiscard]] std::optional<creature_obstacles::Thing> ThingOf(const Registry& registry, entt::entity entity);

/// A creature as what it walks round sees it, by how far round its middle it reaches and where it stands
[[nodiscard]] creature_obstacles::Walker WalkerOf(const Registry& registry, entt::entity entity,
                                                  const components::Creature& body, float radius, glm::vec2 position);

} // namespace openblack::ecs::creature_obstacle_things
