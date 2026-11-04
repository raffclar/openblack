/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ForcedTurnPoint.h"

#include <cmath>

#include "DefaultWorldCameraModel.h"
#include "FightCameraModel.h"
#include "ScriptCameraModel.h"

using namespace openblack;

float forced_turn::Distance(glm::vec3 point, glm::vec3 origin)
{
	const float dx = point.x - origin.x;
	const float dy = point.y - origin.y;
	const float dz = point.z - origin.z;
	return std::sqrt(((dz * dz) + (dy * dy)) + (dx * dx));
}

forced_turn::View forced_turn::AboutPoint(glm::vec3 point, glm::vec3 origin, float heading, float pitch)
{
	const float distance = Distance(point, origin);
	return {.origin = script_camera::PointFromDistanceHeadingAndPitch(point, distance, heading, pitch), .focus = point};
}

bool forced_turn::TakesPoint(const CameraModel& model)
{
	return dynamic_cast<const DefaultWorldCameraModel*>(&model) != nullptr ||
	       dynamic_cast<const FightCameraModel*>(&model) != nullptr;
}
