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

namespace openblack::ecs::components
{
struct HandPulled;
}

/// Where the frame draws a thing that moves between the game turns. The turns go by the thing's Transform; the frames
/// keep the drawn place in fields that only the draw reads.
namespace openblack::ecs::drawn_place
{

/// Whether a thing still stands in the world as it stood when the frame drew its pulled pose. When the world moved,
/// turned or sized the thing after that, the frames no longer draw the pulled pose.
[[nodiscard]] bool StandsAsPulled(const components::HandPulled& pulled, const components::Transform& transform);

/// The Transform that the frame draws a thing with: a walking creature, an animal or a shark between its last two turns,
/// a thing in a creature's hand or in a god's hand where the drawn hand is, else the thing's own Transform
[[nodiscard]] components::Transform DrawnTransformOf(const Registry& registry, entt::entity entity,
                                                     const components::Transform& transform);

} // namespace openblack::ecs::drawn_place
