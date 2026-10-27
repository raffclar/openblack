/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include "ECS/ComputerHand.h"
#include "ECS/ComputerHandSteps.h"
#include "ECS/ComputerMindCreatureRules.h"

using namespace openblack;
using namespace openblack::computer_hand;
using namespace openblack::computer_hand::steps;

namespace
{

/// At this distance in metres or less from the head of the creature, the hand is at the head
constexpr float k_AtHeadDistance = 1.0f;

/// The hand aims at the head of the creature: its place, as high as the creature is tall
void AimAtHead(HandAction& action, const World& world, entt::entity creature)
{
	const auto coords = world.moves.CoordsOf(creature);
	if (!coords.has_value())
	{
		return;
	}
	action.target = PlaceOf(world, *coords);
	action.target.y = world.moves.HeightOf(creature) + coords->altitude;
}

Result ToHeadStart(HandAction& action, const World& world, const Step& step)
{
	AimAtHead(action, world, ArgumentAt(step, 0).object);
	return Result::Next;
}

Result ToHeadProcess(HandAction& action, const World& world, const Step& step)
{
	const auto creature = ArgumentAt(step, 0).object;
	AimAtHead(action, world, creature);
	StepTowardTarget(action);
	const float dx = action.hand.x - action.target.x;
	const float dz = action.hand.z - action.target.z;
	const float rest = std::sqrt(dz * dz + dx * dx);
	// The hand floats as high over the land as the creature is tall
	const float height = world.moves.HeightOf(creature);
	action.hand.y = world.moves.LandHeight(UnderHand(action)) + height;
	return rest > k_AtHeadDistance ? Result::Continue : Result::Next;
}

Result GiveProcess(HandAction& action, const World& world, const Step& step)
{
	// The hand throws what it holds to the creature, as a throw at its place
	if (const auto coords = world.moves.CoordsOf(ArgumentAt(step, 0).object))
	{
		world.objects.ThrowHeld(action.hand, ThrowVelocity(action.hand, PlaceOf(world, *coords)));
	}
	return Result::Next;
}

Result ForceActionStart(HandAction& /*action*/, const World& world, const Step& step)
{
	const auto action = static_cast<uint32_t>(ArgumentAt(step, 1).number);
	static_cast<void>(world.creature.ForceAction(ArgumentAt(step, 0).object, action, ArgumentAt(step, 2).object));
	return Result::Next;
}

Result ForceActionProcess(HandAction& action, const World& world, const Step& step)
{
	// The hand stays over the creature while the creature does the order
	const auto creature = ArgumentAt(step, 0).object;
	if (!world.creature.SpeedsOf(creature).has_value())
	{
		return Result::Next;
	}
	KeepHandOver(action, world, creature);
	return world.creature.PlanForced(creature) ? Result::Continue : Result::Next;
}

Result ForceDesireStart(HandAction& action, const World& world, const Step& step)
{
	world.creature.ForceDominantDesire(ArgumentAt(step, 0).object, ArgumentAt(step, 1).number, ArgumentAt(step, 2).value);
	action.counter = 0;
	return Result::Next;
}

Result JourneyProcess(HandAction& /*action*/, const World& world, const Step& step)
{
	const auto creature = ArgumentAt(step, 0).object;
	const auto speeds = world.creature.SpeedsOf(creature);
	const auto coords = world.moves.CoordsOf(creature);
	if (!speeds.has_value() || !coords.has_value())
	{
		return Result::Next;
	}
	const bool longJourney = computer_mind_rules::LongJourney(PlaceOf(world, *coords), ArgumentAt(step, 1).place);
	// The game draws a number for a short journey and does not use it
	if (!longJourney)
	{
		world.creature.DrawForShortJourney();
	}
	world.creature.SetRequiredSpeed(creature, computer_mind_rules::JourneySpeed(longJourney, speeds->walk, speeds->run));
	return Result::Next;
}

} // namespace

void openblack::computer_hand::steps::AddCreatureSteps(StepTable& table)
{
	// SetLeash, AttachLeashToObject, SlapCreature and StrokeCreature are not made yet
	constexpr auto k_Creature = StepOwner::Creature;
	AddStep(table, k_Creature, StepKind::MoveHandToCreatureHead,
	        {.made = true, .start = ToHeadStart, .process = ToHeadProcess});
	AddStep(table, k_Creature, StepKind::GiveObjectInHandToCreature, {.made = true, .process = GiveProcess});
	AddStep(table, k_Creature, StepKind::ForceCreatureToPerformAction,
	        {.made = true, .start = ForceActionStart, .process = ForceActionProcess});
	AddStep(table, k_Creature, StepKind::ForceCreatureDominantDesire, {.made = true, .start = ForceDesireStart});
	AddStep(table, k_Creature, StepKind::SetCreatureSpeedForJourney, {.made = true, .process = JourneyProcess});
}
