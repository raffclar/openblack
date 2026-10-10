/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TemplePen.h"

#include <cmath>
#include <cstdint>

#include <algorithm>

#include "3D/MapCoords.h"

using namespace openblack;

namespace
{
/// A metre value kept as a map position and read back: the product is exact (two floats), then truncated
float AsMapPosition(float metres)
{
	const auto fixed = static_cast<double>(metres) * static_cast<double>(map_coords::k_FixedPerMetre);
	if (!(fixed > -2147483648.0 && fixed < 2147483648.0))
	{
		return map_coords::ToMetres(static_cast<int32_t>(0x80000000u));
	}
	return map_coords::ToMetres(static_cast<int32_t>(fixed));
}
} // namespace

glm::vec2 temple_pen::MapPlace(glm::vec3 point)
{
	return {AsMapPosition(point.x), AsMapPosition(point.z)};
}

bool temple_pen::BetweenWalls(glm::vec2 heart, float heartYAngle, glm::vec2 point)
{
	// Worked in double, as the game keeps these in its FPU's wider registers; only the cosine is rounded to a float
	const double dx = static_cast<double>(point.x) - static_cast<double>(heart.x);
	const double dz = static_cast<double>(point.y) - static_cast<double>(heart.y);
	double angle = static_cast<double>(heartYAngle + k_FirstWallTurn);
	bool inside = true;
	for (int wall = 0; wall < 2; ++wall)
	{
		const double sine = std::sin(angle);
		const auto cosine = static_cast<double>(static_cast<float>(std::cos(angle)));
		// Each wall runs out from the heart along its turn; the point must be on the pen's side of both
		const double side = wall == 0 ? (cosine * dz) + (-sine * dx) : (-cosine * dz) + (sine * dx);
		if (side < 0.0)
		{
			inside = false;
		}
		angle += static_cast<double>(k_WallsApart);
	}
	return inside;
}

float temple_pen::ShownSize(float size, float distanceToHome, bool betweenWalls)
{
	if (!(distanceToHome <= k_OuterRadius) || !betweenWalls)
	{
		return size;
	}
	const float distance = std::max(std::min(distanceToHome, k_OuterRadius), k_InnerRadius);
	const double eased = ((static_cast<double>(distance) - static_cast<double>(k_InnerRadius)) *
	                      (static_cast<double>(size) - static_cast<double>(k_PenSize))) /
	                         (static_cast<double>(k_OuterRadius) - static_cast<double>(k_InnerRadius)) +
	                     static_cast<double>(k_PenSize);
	return static_cast<float>(eased);
}
