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

namespace openblack
{
class LandIslandInterface;
}

/// The warm glow of the hand's light on the water at night: a horizontal 120 x 120 quad on the sea level under the
/// hand, drawn with an additive material before the sea, so it shows through it like the rest of what is under the
/// water.
namespace openblack::hand_water_glow
{

inline constexpr float k_HalfSize = 60.0f;
inline constexpr float k_CellReach = 70.0f; ///< 60 + 10: the half size and one cell
inline constexpr uint8_t k_LowAltitude = 5; ///< a cell altitude byte under 5 is low enough
inline constexpr uint32_t k_MaxAlpha = 190;
/// UV rectangle in the atmos texture: a 12 x 12 texel spot of the 256 x 256 atmos.raw
inline constexpr glm::vec2 k_UvMinimum {0.75f, 0.375f};
inline constexpr glm::vec2 k_UvMaximum {0.796875f, 0.421875f};

/// The hand light's strength, clamp((120 - mean of the base colour) / 15, 0, 1), 0 when the mean is 120 or more.
/// @param baseColour the light-table base colour of this frame, 0..1
[[nodiscard]] float Intensity(const glm::vec3& baseColour);

/// Palette row 6 moved a quarter of the way to (255, 128, 64) per channel (integer, rounded down), alpha
/// clamp(trunc(intensity * 190), 0, 190). @return 0xAARRGGBB
[[nodiscard]] uint32_t Colour(uint32_t warmRampTop, float intensity);

/// Some cell of [(x - 70) / 10, (x + 70) / 10] x [(z - 70) / 10, (z + 70) / 10] has an altitude under 5 or no
/// landscape cell (the first bounds clamped to 0..511, the last ones not)
[[nodiscard]] bool NearLowLand(const LandIslandInterface& island, glm::vec2 xz);

struct Quad
{
	glm::vec2 centre; ///< world x, z; the quad lies on y = 0
	uint32_t argb;    ///< the colour of its four vertices
};

/// The glow of this frame, if any: intensity > 0.01 and low land near the hand
[[nodiscard]] std::optional<Quad> Compute(const LandIslandInterface& island, const glm::vec3& baseColour, uint32_t warmRampTop,
                                          const glm::vec3& handPosition);

} // namespace openblack::hand_water_glow
