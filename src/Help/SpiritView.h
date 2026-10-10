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

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Help/Spirits.h"

/// The camera as the advisors see it: points on the screen at a depth in front of the camera, and points in the world
/// on the screen. Pure, from the camera's values, so that it can be tested without a camera.
namespace openblack::help::spirits
{

struct SpiritView
{
	glm::vec3 eye {0.0f};
	glm::vec3 right {1.0f, 0.0f, 0.0f};
	glm::vec3 up {0.0f, 1.0f, 0.0f};
	/// Away from the eye
	glm::vec3 forward {0.0f, 0.0f, 1.0f};
	/// How far across and up the view reaches at a depth of 1, each side of the middle
	glm::vec2 halfExtent {1.0f, 0.75f};
	float nearClip {1.0f};
	/// The world to the camera's clipping space, whose w is the depth in front of the camera
	glm::mat4 viewProjection {1.0f};
	Screen screen {};
};

/// The world point under a pixel at a depth in front of the camera, at the near plane for a depth of 0. A negative
/// depth is behind the camera, the point mirrored through the eye.
[[nodiscard]] glm::vec3 PointFromScreen(const SpiritView& view, glm::vec2 pixel, float depth);

/// A world point on the screen, nothing when it is closer than the near plane: its pixel (truncated) and its depth
[[nodiscard]] std::optional<ProjectedPoint> ProjectPoint(const SpiritView& view, const glm::vec3& point);

/// A world point in pixels. Nothing when it is closer than the near plane; off the screen, nothing unless forced, when
/// it is projected as it is, without being kept on the screen.
[[nodiscard]] std::optional<glm::vec2> WorldToPixel(const SpiritView& view, const glm::vec3& point, bool force);

} // namespace openblack::help::spirits
