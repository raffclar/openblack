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

namespace openblack::ecs
{
class Registry;

/// Whether this frame draws a thing: the list of things to draw has it, and the sphere round the box of its model is
/// on the screen. A game without the list or without the model's mesh counts every thing as drawn.
[[nodiscard]] bool DrawnOnScreen(const Registry& registry, entt::entity entity);

} // namespace openblack::ecs
