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

#include <glm/mat4x4.hpp>

#include "ECS/Components/Physics.h"

namespace openblack::physics_draw
{

/// Where a thing is drawn this frame: as it stands, from its body's pose while it moves in the physics, or nowhere once
/// its body has sunk wholly under the sea. Nowhere means in no pass at all, so nothing may be made of it afterwards: not
/// its shadow or reflection, nor a tree's sway or bend.
[[nodiscard]] std::optional<glm::mat4> ModelMatrix(const glm::mat4& standing, const ecs::components::PhysicsDrawPose* pose);

} // namespace openblack::physics_draw
