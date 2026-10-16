/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureTempleDance.h"

#include <algorithm>

using namespace openblack;
using creature_mind::DanceOrder;
using creature_mind::Gaze;
using creature_mind::Movement;
using creature_mind::Step;

namespace
{
Step GoNear(uint32_t thing, float distance)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::GoNearObject, .object = thing, .maxDistance = distance},
	        .gaze = Gaze {.object = thing, .whileGoing = true}};
}

Step Order(DanceOrder::Kind kind, uint32_t count = 0)
{
	return {.kind = Step::Kind::Dance, .dance = {.kind = kind, .count = count}};
}

uint32_t DanceTurns(float turnsPerSecond)
{
	return static_cast<uint32_t>(creature_temple_dance::k_DanceSeconds * turnsPerSecond);
}
} // namespace

float creature_temple_dance::ApproachDistance(float height, float heights)
{
	return std::min(height * heights, k_FurthestApproach);
}

std::vector<Step> creature_temple_dance::DanceOutsideTemple(uint32_t temple, float height, float turnsPerSecond)
{
	return {
	    GoNear(temple, ApproachDistance(height, k_TempleApproachHeights)),
	    Order(DanceOrder::Kind::Start),
	    {.kind = Step::Kind::Move,
	     .seconds = k_FaceHeartSeconds,
	     .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = temple},
	     .gaze = Gaze {.object = temple}},
	    Order(DanceOrder::Kind::Dance, DanceTurns(turnsPerSecond)),
	};
}

std::vector<Step> creature_temple_dance::DanceAroundThing(uint32_t thing, float height, float turnsPerSecond)
{
	return {
	    GoNear(thing, ApproachDistance(height, k_ThingApproachHeights)),
	    Order(DanceOrder::Kind::Start),
	    Order(DanceOrder::Kind::Dance, DanceTurns(turnsPerSecond)),
	};
}
