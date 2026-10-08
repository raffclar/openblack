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
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "GestureMatch.h"

// Where the gesture samples come from: the mouse (no button needed), the camera and the land under the cursor.
// Wiki: docs/bw1-notes/magic.md, "Gestures".

namespace openblack::magic::gestures::sampling
{
/// Screen width / height
[[nodiscard]] float ScreenRatio();
/// The landscape (or sea) point under a pixel
[[nodiscard]] std::optional<glm::vec3> ScreenToLand(glm::vec2 pixel);
/// The camera services of PacketFromResult, now
[[nodiscard]] Projection CurrentProjection();
/// The camera's position changed this frame
[[nodiscard]] bool CameraMoving();

/// Every frame (real seconds): the camera's position for CameraMoving, then the mouse samples. The original adds the
/// time of the mouse events and, past 28 ms, sends a cursor position message that feeds the buffer. Here the frames in
/// which the mouse moved count.
void Update(float realSeconds);

/// OPENBLACK_TEST_GESTURE: pixels played as mouse messages, one per 28 ms, instead of the mouse
void PlayStroke(std::vector<glm::ivec2> pixels);
[[nodiscard]] bool PlayingStroke();
} // namespace openblack::magic::gestures::sampling
