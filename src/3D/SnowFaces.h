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

#include <array>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The snow on the trees and the fences. The game reads the snow texture on them by each face, not by each corner: by the
/// texture coordinates of the corner and by the up part of the normal of the face. The normal of the face is the sum of the
/// normals of its three corners in the file, made unit length. A submesh with bones that reads the snow so shows none.
namespace openblack::snow_faces
{

/// The up part of the normal of a face, from the normals of its three corners in the file, in the game's order of sums
[[nodiscard]] float FaceUp(glm::vec3 first, glm::vec3 second, glm::vec3 third);

/// The up part of the normal of a face, kept in two 16-bit whole numbers of a corner: a low part of 14 bits, then the
/// high part. Together they keep the value in steps of one 2^28th.
[[nodiscard]] std::array<int16_t, 2> PackUp(float up);
/// The up part of the normal of a face from its two 16-bit parts, as the vertex shader reads it
[[nodiscard]] float UnpackUp(int16_t low, int16_t high);

/// Where a corner reads the snow texture: across, 0.4 for each unit of the up part of the face's normal above -1, and
/// 0.2 of the part after the whole of the corner's first texture coordinate; down, 0.2 of the part after the whole of the
/// second. Each part is that of the size of the coordinate, whatever its sign.
[[nodiscard]] glm::vec2 SnowUv(float faceUp, glm::vec2 uv);

} // namespace openblack::snow_faces
