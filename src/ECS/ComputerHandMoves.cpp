/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include "Common/GUtilsDistance.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerHandSteps.h"

using namespace openblack;
using namespace openblack::computer_hand;
using namespace openblack::computer_hand::steps;

namespace
{

Result MoveToObjectStart(HandAction& action, const World& world, const Step& step)
{
	if (const auto coords = world.moves.CoordsOf(ArgumentAt(step, 0).object))
	{
		action.target = PlaceOf(world, *coords);
	}
	return Result::Next;
}

Result MoveToTarget(HandAction& action, const World& world, const Step& /*step*/)
{
	StepTowardTarget(action);
	const float restX = action.hand.x - action.target.x;
	const float restZ = action.hand.z - action.target.z;
	FloatOverLand(action, world);
	return std::sqrt(restZ * restZ + restX * restX) <= k_ArrivedDistance ? Result::Next : Result::Continue;
}

Result MoveToPlaceStart(HandAction& action, const World& /*world*/, const Step& step)
{
	action.target = ArgumentAt(step, 0).place;
	return Result::Next;
}

/// The hand moves a step at its speed toward its target and floats over the land. True when the hand arrives: it is
/// at the target when it is no further than a step.
bool StepToPlace(HandAction& action, const World& world)
{
	const float distance = gutils::GetDistance(action.hand, action.target);
	float dx = action.target.x - action.hand.x;
	float dz = action.target.z - action.hand.z;
	if (dx != 0.0f || dz != 0.0f)
	{
		const float inverse = 1.0f / std::sqrt(dx * dx + dz * dz);
		dx = dx * inverse;
		dz = inverse * dz;
	}
	const float step = action.speed / static_cast<float>(k_TurnsPerSecond);
	if (!(distance > step))
	{
		action.hand = action.target;
		FloatOverLand(action, world);
		return true;
	}
	action.hand.x = dx * step + action.hand.x;
	action.hand.y = 0.0f * step + action.hand.y;
	action.hand.z = dz * step + action.hand.z;
	FloatOverLand(action, world);
	return false;
}

Result MoveToPlace(HandAction& action, const World& world, const Step& /*step*/)
{
	// The hand moves at its speed and stops at the place when it is nearer than a step
	return StepToPlace(action, world) ? Result::Next : Result::Continue;
}

Result MoveToPlaceInInfluence(HandAction& action, const World& world, const Step& /*step*/)
{
	if (StepToPlace(action, world))
	{
		return Result::Next;
	}
	// The hand stops where the god has no influence
	const map_coords::MapCoords under {
	    .x = map_coords::ToFixed(action.hand.x), .z = map_coords::ToFixed(action.hand.z), .altitude = 0.0f};
	return world.moves.InfluenceAt(under) == 0.0f ? Result::Next : Result::Continue;
}

/// The hand moves near its target, but not too near. When it is nearer than the minimum distance, it moves to the side
/// in x and y. Else it moves toward the target at its speed, and it stops when it is no further than the near distance.
Result MoveNear(HandAction& action, const World& world, float sideStep, bool levelTarget)
{
	const float awayX = action.hand.x - action.target.x;
	const float awayZ = action.hand.z - action.target.z;
	const float distance = std::sqrt(awayZ * awayZ + awayX * awayX);
	float dx = action.target.x - action.hand.x;
	float dz = action.target.z - action.hand.z;
	if (!(distance > k_NearMinimum))
	{
		float step = action.speed / static_cast<float>(k_TurnsPerSecond);
		if (sideStep < step)
		{
			step = sideStep;
		}
		action.hand.x = step + action.hand.x;
		action.hand.y = step + action.hand.y;
		action.hand.z = 0.0f + action.hand.z;
		FloatOverLand(action, world);
		return Result::Continue;
	}
	if (dx != 0.0f || dz != 0.0f)
	{
		const float inverse = 1.0f / std::sqrt(dz * dz + dx * dx);
		dx = inverse * dx;
		dz = dz * inverse;
	}
	float step = action.speed / static_cast<float>(k_TurnsPerSecond);
	if (distance < step)
	{
		step = distance;
	}
	const float moveX = step * dx;
	const float moveY = step * 0.0f;
	const float moveZ = dz * step;
	action.hand.x = moveX + action.hand.x;
	action.hand.y = moveY + action.hand.y;
	action.hand.z = moveZ + action.hand.z;
	if (levelTarget)
	{
		action.target.y = 0.0f;
	}
	const float restX = action.hand.x - action.target.x;
	const float restZ = action.hand.z - action.target.z;
	FloatOverLand(action, world);
	return std::sqrt(restZ * restZ + restX * restX) > k_NearDistance ? Result::Continue : Result::Next;
}

Result MoveNearPlace(HandAction& action, const World& world, const Step& /*step*/)
{
	return MoveNear(action, world, k_NearPlaceSideStep, true);
}

Result MoveNearObject(HandAction& action, const World& world, const Step& /*step*/)
{
	return MoveNear(action, world, k_NearObjectSideStep, false);
}

/// The hand moves straight toward its target in all three directions and does not float over the land
Result MoveDirect(HandAction& action, const World& /*world*/, const Step& /*step*/)
{
	const float distance = gutils::GetDistance(action.hand, action.target);
	float dx = action.target.x - action.hand.x;
	float dy = action.target.y - action.hand.y;
	float dz = action.target.z - action.hand.z;
	if (dx != 0.0f || dy != 0.0f || dz != 0.0f)
	{
		const float inverse = 1.0f / std::sqrt(dz * dz + dx * dx + dy * dy);
		dx = dx * inverse;
		dy = inverse * dy;
		dz = dz * inverse;
	}
	const float step = action.speed / static_cast<float>(k_TurnsPerSecond);
	if (!(distance > step))
	{
		action.hand = action.target;
		return Result::Next;
	}
	action.hand.x = dx * step + action.hand.x;
	action.hand.y = dy * step + action.hand.y;
	action.hand.z = dz * step + action.hand.z;
	return Result::Continue;
}

/// The hand goes to the place at once and floats over the land there
Result SetToPlace(HandAction& action, const World& world, const Step& step)
{
	action.target = ArgumentAt(step, 0).place;
	action.hand = action.target;
	FloatOverLand(action, world);
	return Result::Next;
}

/// The hand goes to the place at once, at the height of the place
Result SetToPlaceDirect(HandAction& action, const World& /*world*/, const Step& step)
{
	action.target = ArgumentAt(step, 0).place;
	action.hand = action.target;
	return Result::Next;
}

Result CountStart(HandAction& action, const World& /*world*/, const Step& /*step*/)
{
	action.counter = 0;
	return Result::Next;
}

/// The hand waits until the thing is nearer the place than the distance of the second argument, for one minute at most
Result WaitForNear(HandAction& action, const World& world, const Step& step)
{
	++action.counter;
	const auto most = static_cast<uint32_t>(map_coords::FtoL(static_cast<float>(k_TurnsPerSecond) * k_WaitForNearSeconds));
	if (action.counter > most)
	{
		return Result::Abort;
	}
	const auto coords = world.moves.CoordsOf(ArgumentAt(step, 0).object);
	if (!coords.has_value())
	{
		return Result::Abort;
	}
	const auto at = PlaceOf(world, *coords);
	const auto near = ArgumentAt(step, 1);
	const float dx = at.x - near.place.x;
	const float dy = at.y - near.place.y;
	const float dz = at.z - near.place.z;
	const float distance = std::sqrt(dz * dz + dy * dy + dx * dx);
	return distance < near.value ? Result::Next : Result::Continue;
}

} // namespace

void openblack::computer_hand::steps::AddMoveSteps(StepTable& table)
{
	constexpr auto k_Moves = StepOwner::Moves;
	AddStep(table, k_Moves, StepKind::MoveHandToObject, {.made = true, .start = MoveToObjectStart, .process = MoveToTarget});
	AddStep(table, k_Moves, StepKind::MoveHandToPos, {.made = true, .start = MoveToPlaceStart, .process = MoveToPlace});
	AddStep(table, k_Moves, StepKind::MoveHandNearPos, {.made = true, .start = MoveToPlaceStart, .process = MoveNearPlace});
	AddStep(table, k_Moves, StepKind::MoveHandNearObject,
	        {.made = true, .start = MoveToObjectStart, .process = MoveNearObject});
	AddStep(table, k_Moves, StepKind::MoveHandToPosStopWhenOutOfInfluence,
	        {.made = true, .start = MoveToPlaceStart, .process = MoveToPlaceInInfluence});
	AddStep(table, k_Moves, StepKind::WaitForObjectToBeNearPos, {.made = true, .start = CountStart, .process = WaitForNear});
	AddStep(table, k_Moves, StepKind::SetHandToPos, {.made = true, .process = SetToPlace});
	AddStep(table, k_Moves, StepKind::MoveHandToPosDirectYInterpolation,
	        {.made = true, .start = MoveToPlaceStart, .process = MoveDirect});
	AddStep(table, k_Moves, StepKind::SetHandToPosDirectY, {.made = true, .process = SetToPlaceDirect});
}
