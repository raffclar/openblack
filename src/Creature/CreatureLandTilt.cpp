/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureLandTilt.h"

#include "3D/MapCoords.h"

using namespace openblack;

namespace
{
/// The slope along one axis, kept in the limits as the game keeps it
float Slope(float plus, float minus, float across)
{
	float slope = ((plus - minus) * creature_land_tilt::k_SlopeShare) / across;
	// The game keeps a slope at the top or above at the top. Then it keeps a slope that is not above the bottom at the
	// bottom, and that includes a slope that is not a number.
	if (slope >= creature_land_tilt::k_MostRise)
	{
		slope = creature_land_tilt::k_MostRise;
	}
	if (!(slope > -creature_land_tilt::k_MostRise))
	{
		slope = -creature_land_tilt::k_MostRise;
	}
	return slope;
}
} // namespace

std::array<glm::vec2, 4> creature_land_tilt::SamplePoints(const glm::vec3& position, float radius)
{
	const float back = -radius;
	return {{
	    {radius + position.x, position.z},
	    {position.x, radius + position.z},
	    {back + position.x, position.z},
	    {position.x, back + position.z},
	}};
}

int32_t creature_land_tilt::SampleUnit(float metres)
{
	constexpr float k_Units = 65536.0f;
	constexpr float k_Tenth = std::bit_cast<float>(0x3DCCCCCDu);
	const float units = metres * k_Units;
	return map_coords::FtoL(units * k_Tenth);
}

creature_land_tilt::Tilt creature_land_tilt::FromHeights(const Heights& heights, float radius)
{
	const float across = radius + radius;
	return {.alongX = Slope(heights.plusX, heights.minusX, across), .alongZ = Slope(heights.plusZ, heights.minusZ, across)};
}

creature_land_tilt::Tilt creature_land_tilt::Of(const glm::vec3& position, float radius, bool heldByCreature,
                                                const std::function<float(int32_t, int32_t)>& altitude)
{
	if (heldByCreature)
	{
		return FromHeights({.plusX = position.y, .plusZ = position.y, .minusX = position.y, .minusZ = position.y}, radius);
	}
	const auto points = SamplePoints(position, radius);
	const auto at = [&altitude](const glm::vec2& point) { return altitude(SampleUnit(point.x), SampleUnit(point.y)); };
	return FromHeights({.plusX = at(points[0]), .plusZ = at(points[1]), .minusX = at(points[2]), .minusZ = at(points[3])},
	                   radius);
}

glm::mat3 creature_land_tilt::Lean(const glm::mat3& rotation, const Tilt& tilt)
{
	auto leaned = rotation;
	for (int axis = 0; axis < 3; ++axis)
	{
		const auto& column = rotation[axis];
		const float rise = (column.x * tilt.alongX) + (column.z * tilt.alongZ);
		leaned[axis].y = rise + column.y;
	}
	return leaned;
}

glm::mat4 creature_land_tilt::Placement(const glm::vec3& position, const glm::mat3& rotation, const glm::vec3& scale,
                                        const Tilt& tilt)
{
	glm::mat4 matrix(Lean(rotation, tilt));
	matrix[0] *= scale.x;
	matrix[1] *= scale.y;
	matrix[2] *= scale.z;
	matrix[3] = glm::vec4(position, 1.0f);
	return matrix;
}
