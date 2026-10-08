/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/vec3.hpp>

namespace openblack
{
class LandIslandInterface;
}

/// The land or sea under a pixel of the screen, found as the game finds it: along the line from the camera through the
/// pixel's point on the near plane, with the land's own line test (LandIslandInterface::RayCast).
namespace openblack::land_pick
{

/// The middle of the map and how far from it a point under the cursor may be: points farther away are pulled back onto
/// the sphere of this radius about the middle
inline constexpr float k_MapMiddle = 10.0f * 512.0f * 0.5f;
inline constexpr float k_PickReach = 10.0f * 1536.0f * 0.5f;

/// The land the line from the camera through the near plane's point meets, nearest first. With the sea, failing the land
/// the sea's level near the camera (within 7500 m), and failing that the sea's level wherever the line goes down
/// (provided the camera is above the near point). The point's height is the land's height there (0 at sea), and it is
/// kept within reach of the map's middle. Without the sea, only the land itself.
[[nodiscard]] std::optional<glm::vec3> UnderPixel(const LandIslandInterface& island, glm::vec3 camera, glm::vec3 nearPoint,
                                                  bool withSea);

/// A point pulled onto the sphere of k_PickReach about the map's middle (at height 0) when it lies outside it
[[nodiscard]] glm::vec3 KeptInReach(glm::vec3 point);

} // namespace openblack::land_pick
