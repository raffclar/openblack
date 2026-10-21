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
#include <bit>
#include <functional>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The creature leans its body with the slope of the land under it. The game reads the land a radius away on the four
/// sides, and makes the two flat axes of the body rise with the slope. The up axis stays straight up, so the body is
/// sheared and not turned.
namespace openblack::creature_land_tilt
{

/// The part of the slope that the body takes
inline constexpr float k_SlopeShare = std::bit_cast<float>(0x3F19999Au);
/// The most that one flat axis rises or drops for each metre along it
inline constexpr float k_MostRise = std::bit_cast<float>(0x3E99999Au);

/// How much each flat axis of the world rises for each metre along it, as the body takes it
struct Tilt
{
	float alongX {0.0f};
	float alongZ {0.0f};
};

/// The height of the land at the four points that the game reads, one radius away from the creature
struct Heights
{
	float plusX {0.0f};
	float plusZ {0.0f};
	float minusX {0.0f};
	float minusZ {0.0f};
};

/// The four points on the land, in the order that the game reads them: +x, +z, -x, then -z
[[nodiscard]] std::array<glm::vec2, 4> SamplePoints(const glm::vec3& position, float radius);

/// A position in metres as the map unit that the game reads the height of the land at: the metres times 65536, then
/// times 0.1, each as a float, then cut to a whole number
[[nodiscard]] int32_t SampleUnit(float metres);

/// The tilt from the four heights. Each slope is the difference of two heights, times the share, over twice the radius.
/// Each slope stops at the most rise in the two directions. A slope that is not a number becomes the most drop.
[[nodiscard]] Tilt FromHeights(const Heights& heights, float radius);

/// The tilt of a creature at its position. A creature that another creature holds reads its own height at all four
/// points, so it stays flat. The altitude function takes the two map units of a point.
[[nodiscard]] Tilt Of(const glm::vec3& position, float radius, bool heldByCreature,
                      const std::function<float(int32_t, int32_t)>& altitude);

/// The turn of the body with the tilt. Each axis rises by the tilt along x times its x part, then by the tilt along z
/// times its z part, then by its own up part.
[[nodiscard]] glm::mat3 Lean(const glm::mat3& rotation, const Tilt& tilt);

/// The matrix that places the body in the world: the turn with the tilt, then the scale, then the position
[[nodiscard]] glm::mat4 Placement(const glm::vec3& position, const glm::mat3& rotation, const glm::vec3& scale,
                                  const Tilt& tilt);

} // namespace openblack::creature_land_tilt
