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

#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/Billboard.h"

/// Screen <-> 3D conversions, which work with the depth along the camera's forward axis (camera space z), not with a
/// ray length: the 3D point of a screen pixel and the projection used by sprites and point projection. Shared by the
/// falling spell (Magic/Objects/FallingSpell.cpp, in its own camera space) and the advisor spirits
/// (Help/SpiritsRuntime.cpp, in the world through the drawn camera). Pure maths, no Locator.
///
/// The lens: T = tan(fov / 2), fx = 1 / T, fy = aspect / T with aspect = W / H; the screen to 3D conversion uses near T
/// and near T / aspect.
namespace openblack::screen_point
{

struct Lens
{
	float halfW;
	float halfH;
	float tanHalf;
	float aspect;
};

/// The lens of a W x H screen and a horizontal field of view in radians (Camera::GetHorizontalFieldOfView)
[[nodiscard]] Lens LensOf(int width, int height, float horizontalFov);

/// The 3D point of a pixel in camera space: ((x - hW) near T / hW, (hH - y) near T / aspect / hH) x depth / near,
/// z = depth (the camera's rotation and position then take it to the world). (approximate) the near
/// cancels out; the original's rounding goes through it
[[nodiscard]] glm::vec3 CameraPointFromScreen(const Lens& lens, int32_t x, int32_t y, float depth);

/// The sprite projection without clipping: sx = (X / Z + 1) hW, sy = hH - Y / Z hH, X = fx x, Y = fy y
[[nodiscard]] glm::vec2 ProjectCamera(const Lens& lens, const glm::vec3& v);

/// The camera-space point of a world point: worldToCamera (p - eye)
[[nodiscard]] glm::vec3 ToCamera(const graphics::billboard::CameraFrame& frame, const glm::vec3& world);
/// The world point of a camera-space point: eye + right x + up y + forward z
[[nodiscard]] glm::vec3 ToWorld(const graphics::billboard::CameraFrame& frame, const glm::vec3& camera);

/// The 3D point of a pixel in the world: the pixel at that camera depth
[[nodiscard]] glm::vec3 PointFromScreen(const graphics::billboard::CameraFrame& frame, const Lens& lens, int32_t x, int32_t y,
                                        float depth);

/// What the point projection gives: the pixel and the camera depth. (inferred) nullopt for a point at or
/// behind the eye (z <= 0), where the division would fail; the original's other outputs are not read
struct Projected
{
	glm::vec2 pixel {0.0f};
	float depth {0.0f};
};
[[nodiscard]] std::optional<Projected> Project(const graphics::billboard::CameraFrame& frame, const Lens& lens,
                                               const glm::vec3& world);

} // namespace openblack::screen_point
