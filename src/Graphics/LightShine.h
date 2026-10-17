/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>

#include <glm/vec2.hpp>

namespace openblack::graphics::light_shine
{

/// A light that shines in a colour of its own, as the light out of an opened reward chest does. Its colour takes the
/// place of the land's light and of the sun's shade, and the distance haze does not dim it; the land's colour and the
/// haze are still added. It is solid on the vertices of its primitive's second group and clear on all others, and its
/// texture turns about its middle with the real time.

/// How far the texture has turned after this much real time, in radians: one full turn each 20 seconds, in the
/// negative direction. The real time does not stop while the game is paused. The angle is a float that grows with the
/// time; the caller takes its cosine and sine.
[[nodiscard]] double TextureTurn(std::chrono::milliseconds realTime);

/// Where a vertex reads the texture: its coordinates about the middle of the texture, mirrored top to bottom and then
/// turned by the angle (given by its cosine and sine)
[[nodiscard]] glm::vec2 TurnedUv(glm::vec2 uv, float cosTurn, float sinTurn);

/// The alpha of a vertex, 0 to 1: solid on the bone of the primitive's second group of vertices, clear on all others
[[nodiscard]] float VertexAlpha(int vertexBone, int solidBone);

} // namespace openblack::graphics::light_shine
