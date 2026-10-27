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
#include "ECS/ComputerMindRules.h"

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

/// The hand makes food or wood in its hand from a container: a store, a pile or a pot
Result FromContainerProcess(const HandAction& action, const World& world, const Step& step, ResourceType type)
{
	const auto inPot = world.objects.HeldPotHolds(type);
	// A hand that holds something else waits until it is empty
	if (!world.objects.HandHasRoom() && !inPot.has_value())
	{
		return Result::Continue;
	}
	const auto container = ArgumentAt(step, 0).object;
	const auto take =
	    computer_mind_rules::ContainerTake(ArgumentAt(step, 1).number, inPot, world.objects.ResourceIn(container, type));
	bool hasPot = inPot.has_value();
	if (take > 0)
	{
		hasPot = world.objects.TakeFromContainer(container, type, static_cast<uint32_t>(take), action.hand) || hasPot;
	}
	return hasPot ? Result::Next : Result::Abort;
}

Result FoodFromContainerProcess(HandAction& action, const World& world, const Step& step)
{
	return FromContainerProcess(action, world, step, ResourceType::Food);
}

Result WoodFromContainerProcess(HandAction& action, const World& world, const Step& step)
{
	return FromContainerProcess(action, world, step, ResourceType::Wood);
}

} // namespace

void openblack::computer_hand::steps::AddObjectSteps(StepTable& table)
{
	// ThrowObjectInHandAtPos is not made yet
	AddStep(table, StepOwner::Objects, StepKind::PickUpObject, {.made = true, .start = PickUpStart, .process = PickUpProcess});
	AddStep(table, StepOwner::Objects, StepKind::DropObjectInHandAtPos, {.made = true, .process = DropHeldProcess});
	AddStep(table, StepOwner::Objects, StepKind::DropObjectInHandOnObject, {.made = true, .process = UseHeldProcess});
	AddStep(table, StepOwner::Objects, StepKind::CreateFoodFromContainer, {.made = true, .process = FoodFromContainerProcess});
	AddStep(table, StepOwner::Objects, StepKind::CreateWoodFromContainer, {.made = true, .process = WoodFromContainerProcess});
}
