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

#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// EXPERIMENT (not for merge): the weather of the whole planet for the volumetric clouds. A map on the sphere gives the
/// cloud cover, the type of cloud and the fronts at each place on the planet. It has large weather systems: bands of
/// latitude, cyclones with spiral arms, fronts and clear areas, over noise that has no repeat on the sphere. The clouds
/// read it from a cube map, near the eye and from space, so that no pattern repeats at any height.
namespace openblack::graphics::planet_weather
{

/// The side of each face of the cube map, in texels (about 20 km for each texel)
inline constexpr uint32_t k_CubeSide = 512;
/// The latitude of the island, in degrees north
inline constexpr float k_IslandLatitude = 40.0f;
/// The number of cyclones on the planet
inline constexpr uint32_t k_Cyclones = 16;
/// The middle of the map: every land is 5120 units square from 0. The planet's middle is under it.
inline constexpr glm::vec2 k_MapMiddle {2560.0f, 2560.0f};

/// The weather at a place on the planet
struct Weather
{
	/// How much cloud there is, 0 (clear) to 1
	float cover {0.0f};
	/// The type of cloud: 0 flat layers, 1 heaped and tall clouds
	float type {0.5f};
	/// How strong a front or the arm of a cyclone is there, 0 to 1
	float front {0.0f};
	/// A slow value from 0 to 1 that chooses the size of the cells of cloud: 0 small, 1 large
	float pattern {0.5f};
};

/// The direction from the planet's middle towards the north pole, in the world's axes (y up at the island, z north)
[[nodiscard]] glm::vec3 NorthPole();
/// The latitude of a direction from the planet's middle, in degrees
[[nodiscard]] float LatitudeOf(glm::vec3 direction);
/// The change of the cover with the latitude: cloud at the equator and over the storm tracks, clear air over the
/// subtropics. About -0.25 to 0.2.
[[nodiscard]] float LatitudeBands(float latitude);
/// The weather at a direction from the planet's middle (length 1)
[[nodiscard]] Weather WeatherAt(glm::vec3 direction);
/// The direction from the cube's middle through a texel of a face (bgfx's order: +x, -x, +y, -y, +z, -z), with uv from
/// 0 to 1 across the face
[[nodiscard]] glm::vec3 CubeDirection(uint32_t face, glm::vec2 uv);
/// The cube map, RGBA8, six faces of `side` texels a side, x fastest: r the cover, g the type, b the fronts, a the
/// pattern. The faces are made on threads at the same time.
[[nodiscard]] std::vector<uint8_t> CubeTexels(uint32_t side = k_CubeSide);
/// The direction from the planet's middle to a point across the ground, `fromMiddle` world units from the map's middle,
/// on a planet of a radius
[[nodiscard]] glm::vec3 DirectionAt(glm::vec2 fromMiddle, float radius);

} // namespace openblack::graphics::planet_weather
