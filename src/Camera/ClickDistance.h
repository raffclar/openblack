/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <bit>
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// How far ahead of the world camera the land is, as a press of the mouse finds it. The drags, the zoom and the
/// single tap of the temple key use this distance.
namespace openblack::click_distance
{

/// The number of rays down the middle of the screen
inline constexpr int32_t k_Rays = 0x10;
/// The distance that counts as one more hit, so that the mean is this when no ray hits the land
inline constexpr float k_NoHitDistance = 50.0f;
/// How much each unit of the pitch moves the distance towards the focus distance
inline constexpr float k_PitchShare = std::bit_cast<float>(0x3D594CAFu);

/// Where a ray goes through the screen, as a share of its width and height from the top left: the middle column, and
/// row `ray` of 16 from the top down. At the game's screen sizes these are the game's whole pixels.
[[nodiscard]] glm::vec2 RayCoord(int32_t ray);
/// The distance from the camera to the point where a ray hits the land
[[nodiscard]] float HitDistance(const glm::vec3& origin, const glm::vec3& hit);
/// The harmonic mean of the distances of the hits, with one more hit at the no-hit distance
[[nodiscard]] float MeanDistance(std::span<const float> hitDistances);
/// The mean distance moved towards the focus distance, more as the camera looks more steeply down
[[nodiscard]] float TowardsFocus(float meanDistance, float focusDistance, const glm::vec3& origin, const glm::vec3& focus);

} // namespace openblack::click_distance
