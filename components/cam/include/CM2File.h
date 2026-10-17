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
#include <optional>
#include <span>
#include <vector>

/*
 * The layout of a .cm2 camera path, as the falling spell's film camera uses it:
 *
 * - header: the size of the file, the duration in milliseconds over which the points are evenly spread, and the
 *   number of points, each a 32-bit unsigned int
 * - points: for each point, 18 floats
 *   - position: where the camera is (3 floats)
 *   - focus: the point the camera looks at (3 floats)
 *   - frame: the camera's axes, three rows of 3 floats, then its position again (12 floats)
 */
namespace openblack::cam
{

struct CM2Point
{
	std::array<float, 3> position;
	std::array<float, 3> focus;
	/// The camera's axes as three rows (right, up and back), then the camera's position
	std::array<float, 12> frame;
};
static_assert(sizeof(CM2Point) == 0x48);

struct CM2Path
{
	uint32_t durationMilliseconds {0};
	std::vector<CM2Point> points;
};

/// The path in a .cm2 file's bytes; none when the bytes are too few for the header or for the points it counts, or
/// when it has no points
[[nodiscard]] std::optional<CM2Path> ReadCM2(std::span<const uint8_t> bytes);

} // namespace openblack::cam
