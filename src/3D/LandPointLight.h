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
#include <functional>
#include <optional>
#include <span>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

/// The light of the land at one point, as the game gives it to a thing that it makes there, and the distance haze on
/// that colour. All colours are 0xAARRGGBB.
namespace openblack::land_point_light
{

/// The luminosity of the four corners of a cell: the cell, the cell after it on z, the cell after it on x, and the cell
/// after it on both
using Corners = std::array<uint8_t, 4>;
/// The corners of the cell at a cell position, none when the cell is off the map or has no block of land
using CornersAt = std::function<std::optional<Corners>(int32_t x, int32_t z)>;

/// The land's light of the frame: one colour for each level of luminosity
using LightTable = std::span<const uint32_t, 256>;

/// The distance haze of the frame
struct Haze
{
	/// Whether the haze is on
	bool on {false};
	/// From this depth in front of the camera to the far depth, a colour fades to k of 256 of itself
	float nearDepth {0.0f};
	float farDepth {0.0f};
	int32_t k {256};
};

/// A colour moved towards another by a fraction of 256 on each channel, rounded down, with an alpha of 255
[[nodiscard]] uint32_t Blend(uint32_t from, uint32_t to, int32_t fraction);

/// The part of a cell from its corner to a point, in 256ths: the point in cells less the whole cell, times 256, cut
/// towards zero
[[nodiscard]] int32_t CellFraction(float cells, int32_t cell);

/// The land's light at a point. The colour of each corner's luminosity is blended along z, then along x. Off the map,
/// or where there is no land, the light is the colour of the brightest level.
[[nodiscard]] uint32_t LightAt(glm::vec3 point, LightTable table, const CornersAt& corners);

/// The depth of a point in front of the camera, from the matrix that takes the world to the screen
[[nodiscard]] float DepthOf(glm::vec3 point, const glm::mat4& worldToClip);

/// A colour as the distance haze dims it at a depth in front of the camera. The alpha does not change.
[[nodiscard]] uint32_t Hazed(uint32_t colour, float depth, const Haze& haze);

} // namespace openblack::land_point_light
