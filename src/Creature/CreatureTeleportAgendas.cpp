/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureTeleportAgendas.h"

#include "3D/MapCoords.h"
#include "Magic/TeleportRules.h"

using namespace openblack::creature_mind;

namespace
{
/// A walk to a point, which ends within the reach of a teleport walk, with the head on the ground below the point
Step WalkTo(glm::vec2 point)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::ToPoint,
	                     .point = point,
	                     .maxDistance = openblack::magic::teleport::k_CreatureArriveDistance},
	        .gaze = Gaze {.point = glm::vec3(point.x, 0.0f, point.y), .pointOnGround = true}};
}
} // namespace

std::vector<Step> openblack::creature_mind::GoToTeleport(uint32_t stone, glm::vec2 stonePoint)
{
	const glm::vec2 point {map_coords::Quantise(stonePoint.x), map_coords::Quantise(stonePoint.y)};
	auto walk = WalkTo(point);
	walk.face = creature_face::Cue::Puzzled;
	// The game also adds a short wait with a puzzled face after the jump. No turn ever gets to it: a refused jump stops
	// the plan, and a jump gives the creature a new plan.
	return {walk, {.kind = Step::Kind::Teleport, .object = stone, .main = true}};
}

std::vector<Step> openblack::creature_mind::AfterTeleport(std::optional<glm::vec2> destination)
{
	std::vector<Step> agenda {{.kind = Step::Kind::Transported}};
	if (destination.has_value())
	{
		agenda.push_back(WalkTo(*destination));
	}
	agenda.push_back({.kind = Step::Kind::StopTeleportControl});
	return agenda;
}
