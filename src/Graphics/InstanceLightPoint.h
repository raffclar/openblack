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

/// The y of an instance's look below 0 (in place of a colour of its own): the instance takes no colour of the land. The
/// parts on the deck of the explorers' boat take only the land's light of the boat.
inline constexpr float k_NoLandColour = -1.0f;

/// The z of an instance's look above 0 and at most a half (in place of a fade, below 0, or hidden, above a half): the
/// instance takes the land's light and colour but no distance haze, as a villager that a script draws in high detail
inline constexpr float k_NoHaze = 0.25f;

/// The z of an instance's look for an instance that takes no distance haze: k_NoHaze in place of 0, a fade or a hidden
/// instance as it is
[[nodiscard]] constexpr float WithoutHaze(float lookZ) noexcept
{
	return lookZ == 0.0f ? k_NoHaze : lookZ;
}

void SetLightOffset(glm::mat4& model, glm::vec2 offset) noexcept;
[[nodiscard]] glm::vec2 LightOffset(const glm::mat4& model) noexcept;

} // namespace openblack::graphics::instance_light
