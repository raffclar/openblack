/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ECS/ComputerHand.h"
#include "ECS/ComputerHandSteps.h"

using namespace openblack;
using namespace openblack::computer_hand;
using namespace openblack::computer_hand::steps;

namespace
{

Result PickUpStart(HandAction& action, const World& world, const Step& step)
{
	if (const auto coords = world.moves.CoordsOf(ArgumentAt(step, 0).object))
	{
		action.target = PlaceOf(world, *coords);
	}
	return Result::Next;
}

Result PickUpProcess(HandAction& action, const World& world, const Step& step)
{
	// A full hand puts down what it holds first, and tries again on the next turn
	if (!world.objects.HandHasRoom())
	{
		world.objects.PutDownHeld(action.hand);
		return Result::Continue;
	}
	const auto thing = ArgumentAt(step, 0).object;
	if (!world.objects.CanPickUp(thing))
	{
		return Result::Abort;
	}
	world.objects.PickUp(thing, action.hand);
	return Result::Next;
}

Result DropHeldProcess(HandAction& action, const World& world, const Step& /*step*/)
{
	world.objects.DropHeld(action.hand);
	return Result::Next;
}

Result UseHeldProcess(HandAction& action, const World& world, const Step& step)
{
	world.objects.UseHeldOn(ArgumentAt(step, 0).object, action.hand);
	return Result::Next;
}

Result ThrowProcess(HandAction& action, const World& world, const Step& step)
{
	world.objects.ThrowHeld(action.hand, ThrowVelocity(action.hand, ArgumentAt(step, 0).place));
	return Result::Next;
}

} // namespace

void openblack::computer_hand::steps::AddObjectSteps(StepTable& table)
{
	// CreateFoodFromContainer and CreateWoodFromContainer are not made yet
	AddStep(table, StepOwner::Objects, StepKind::PickUpObject, {.made = true, .start = PickUpStart, .process = PickUpProcess});
	AddStep(table, StepOwner::Objects, StepKind::DropObjectInHandAtPos, {.made = true, .process = DropHeldProcess});
	AddStep(table, StepOwner::Objects, StepKind::DropObjectInHandOnObject, {.made = true, .process = UseHeldProcess});
	AddStep(table, StepOwner::Objects, StepKind::ThrowObjectInHandAtPos, {.made = true, .process = ThrowProcess});
}
