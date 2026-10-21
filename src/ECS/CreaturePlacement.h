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
#include <glm/mat4x4.hpp>

#include "Creature/CreatureLandTilt.h"

namespace openblack::ecs
{
class Registry;
namespace components
{
struct Transform;
}
} // namespace openblack::ecs

/// The matrix that places a creature's body in the world as the game places it: its turn, leaned with the slope of
/// the land under it, its scale and its position
namespace openblack::ecs::creature_placement
{

/// The tilt of the creature with the land under it now, by the radius of its body on the ground. With no land, the
/// creature has no tilt.
[[nodiscard]] creature_land_tilt::Tilt TiltOf(const Registry& registry, entt::entity creature,
                                              const components::Transform& transform);

/// The matrix that places the creature's body in the world, with its tilt
[[nodiscard]] glm::mat4 PlacementOf(const Registry& registry, entt::entity creature, const components::Transform& transform);

} // namespace openblack::ecs::creature_placement
