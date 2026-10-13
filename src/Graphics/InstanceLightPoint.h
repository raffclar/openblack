/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

/// An instance can take the land's light and the land's colour from a point that is not its own place, as the lanterns
/// on the Norse gate take the light of the gate. The instance keeps the offset from its place to that point on the land
/// in the w of the second and third columns of its model matrix. These are always 0 for a matrix that places an object.
/// The vertex shader reads the offset and then sets these values to 0 again. An offset of 0 gives no other point.
namespace openblack::graphics::instance_light
{

void SetLightOffset(glm::mat4& model, glm::vec2 offset) noexcept;
[[nodiscard]] glm::vec2 LightOffset(const glm::mat4& model) noexcept;

} // namespace openblack::graphics::instance_light
