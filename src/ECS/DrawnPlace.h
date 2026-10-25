/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "ECS/Components/Transform.h"

namespace openblack::ecs
{
class Registry;
}

/// Where the frame draws a thing that moves between the game turns. The turns go by the thing's Transform; the frames
/// keep the drawn place in fields that only the draw reads.
namespace openblack::ecs::drawn_place
{

/// The Transform that the frame draws a thing with: a walking creature, an animal or a shark between its last two turns,
/// a thing in a creature's hand where the drawn hand is, else the thing's own Transform
[[nodiscard]] components::Transform DrawnTransformOf(const Registry& registry, entt::entity entity,
                                                     const components::Transform& transform);

} // namespace openblack::ecs::drawn_place
