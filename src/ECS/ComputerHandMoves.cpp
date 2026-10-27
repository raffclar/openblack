/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

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

Result MoveToPlace(HandAction& action, const World& world, const Step& /*step*/)
{
	// The hand moves at its speed and stops at the place when it is nearer than a step
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
		return Result::Next;
	}
	action.hand.x = dx * step + action.hand.x;
	action.hand.y = 0.0f * step + action.hand.y;
	action.hand.z = dz * step + action.hand.z;
	FloatOverLand(action, world);
	return Result::Continue;
}

} // namespace

void openblack::computer_hand::steps::AddMoveSteps(StepTable& table)
{
	// MoveHandNearPos, MoveHandNearObject, MoveHandToPosStopWhenOutOfInfluence, WaitForObjectToBeNearPos, SetHandToPos,
	// MoveHandToPosDirectYInterpolation and SetHandToPosDirectY are not made yet
	AddStep(table, StepOwner::Moves, StepKind::MoveHandToObject,
	        {.made = true, .start = MoveToObjectStart, .process = MoveToTarget});
	AddStep(table, StepOwner::Moves, StepKind::MoveHandToPos,
	        {.made = true, .start = MoveToPlaceStart, .process = MoveToPlace});
}
