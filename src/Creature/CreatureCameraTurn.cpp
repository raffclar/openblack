/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureCameraTurn.h"

#include <cmath>

#include "3D/MapCoords.h"

using namespace openblack;

glm::vec2 creature_camera_turn::OffsetToCamera(glm::vec2 creature, glm::vec2 camera)
{
	return camera - map_coords::ToMetres(map_coords::FromMetres(creature));
}

float creature_camera_turn::GroundLength(glm::vec2 offset)
{
	const float across = offset.x * offset.x;
	const float along = offset.y * offset.y;
	return std::sqrt(across + along);
}

bool creature_camera_turn::Facing(GameHeading body, GameHeading target)
{
	// A difference that is not a number counts as facing
	const float off = body.Radians() - target.Radians();
	return !(std::abs(off) >= k_FacingRadians);
}
