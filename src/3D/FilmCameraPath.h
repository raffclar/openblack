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
#include <vector>

#include <glm/vec3.hpp>

namespace openblack
{
namespace cam
{
struct CM2Path;
}

/// A film's camera path, as the falling spell's film moves its camera round the creature: points spread evenly over a
/// duration, each with the camera's position, the point it looks at, and its axes. A time between two points blends
/// the two points.
class FilmCameraPath
{
public:
	struct Point
	{
		glm::vec3 position {0.0f};
		glm::vec3 focus {0.0f};
		/// The camera's axes as three rows (right, up and back), then its position
		std::array<float, 12> frame {};
	};

	FilmCameraPath() = default;
	explicit FilmCameraPath(const cam::CM2Path& path);

	[[nodiscard]] uint32_t GetDurationMs() const { return _durationMs; }
	[[nodiscard]] const std::vector<Point>& GetPoints() const { return _points; }

	/// The camera at a time into the path, in milliseconds. A time past the end takes the last millisecond. The
	/// fraction of the way to the next point counts the whole milliseconds of each span between points.
	[[nodiscard]] Point SampleAt(uint32_t timeMs) const;

private:
	uint32_t _durationMs {0};
	std::vector<Point> _points;
};

} // namespace openblack
