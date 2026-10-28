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

Result SetLeashProcess(HandAction& /*action*/, const World& world, const Step& step)
{
	const auto creature = ArgumentAt(step, 0).object;
	if (world.creature.SpeedsOf(creature).has_value() && world.creature.HasPlayer(creature))
	{
		world.creature.SetLeashType(creature, static_cast<LeashType>(ArgumentAt(step, 1).number));
	}
	return Result::Next;
}

/// The creature can take the leash of its player
bool Leashable(const World& world, entt::entity creature)
{
	return world.creature.SpeedsOf(creature).has_value() && world.creature.HasPlayer(creature);
}

Result AttachStart(HandAction& /*action*/, const World& world, const Step& step)
{
	const auto creature = ArgumentAt(step, 0).object;
	if (!Leashable(world, creature))
	{
		return Result::Abort;
	}
	// A leash that is tied to a thing comes off before the new tie
	world.creature.TakeOffTiedLeash(creature);
	return Result::Next;
}

Result AttachProcess(HandAction& action, const World& world, const Step& step)
{
	const auto creature = ArgumentAt(step, 0).object;
	if (!Leashable(world, creature) || !world.creature.TieLeash(creature, ArgumentAt(step, 1).object))
	{
		return Result::Abort;
	}
	action.counter = computer_mind_rules::k_TieFollowTurns;
	return Result::Next;
}

Result AttachEnd(HandAction& action, const World& world, const Step& step)
{
	// The hand follows the thing for a time, and then until it is near the thing
	const auto object = ArgumentAt(step, 1).object;
	KeepHandOver(action, world, object);
	if (action.counter > 0)
	{
		--action.counter;
		return Result::Continue;
	}
	const auto coords = world.moves.CoordsOf(object);
	if (!coords.has_value())
	{
		return Result::Continue;
	}
	const map_coords::MapCoords hand {.x = map_coords::ToFixed(action.hand.x), .z = map_coords::ToFixed(action.hand.z)};
	return gutils::GetDistanceInMetres(hand, *coords) < computer_mind_rules::k_TieNearDistance ? Result::Next
	                                                                                           : Result::Continue;
}

/// The creature feels the stroke or the slap, and the hand stays on it for a random time
Result Touch(HandAction& action, const World& world, const Step& step, float feedback)
{
	const auto creature = ArgumentAt(step, 0).object;
	if (world.creature.SpeedsOf(creature).has_value())
	{
		world.creature.Feedback(creature, feedback);
	}
	action.counter = computer_mind_rules::FeedbackTurns(world.creature.DrawFeedbackSeconds());
	return Result::Next;
}

Result SlapProcess(HandAction& action, const World& world, const Step& step)
{
	return Touch(action, world, step, computer_mind_rules::k_SlapFeedback);
}

Result StrokeProcess(HandAction& action, const World& world, const Step& step)
{
	return Touch(action, world, step, computer_mind_rules::k_StrokeFeedback);
}

Result TouchEnd(HandAction& action, const World& world, const Step& step)
{
	KeepHandOver(action, world, ArgumentAt(step, 0).object);
	--action.counter;
	return action.counter != 0 ? Result::Continue : Result::Next;
}

} // namespace

void openblack::computer_hand::steps::AddCreatureSteps(StepTable& table)
{
	constexpr auto k_Creature = StepOwner::Creature;
	AddStep(table, k_Creature, StepKind::SetLeash, {.made = true, .process = SetLeashProcess});
	AddStep(table, k_Creature, StepKind::AttachLeashToObject,
	        {.made = true, .start = AttachStart, .process = AttachProcess, .end = AttachEnd});
	AddStep(table, k_Creature, StepKind::SlapCreature, {.made = true, .process = SlapProcess, .end = TouchEnd});
	AddStep(table, k_Creature, StepKind::StrokeCreature, {.made = true, .process = StrokeProcess, .end = TouchEnd});
	AddStep(table, k_Creature, StepKind::MoveHandToCreatureHead,
	        {.made = true, .start = ToHeadStart, .process = ToHeadProcess});
	AddStep(table, k_Creature, StepKind::GiveObjectInHandToCreature, {.made = true, .process = GiveProcess});
	AddStep(table, k_Creature, StepKind::ForceCreatureToPerformAction,
	        {.made = true, .start = ForceActionStart, .process = ForceActionProcess});
	AddStep(table, k_Creature, StepKind::ForceCreatureDominantDesire, {.made = true, .start = ForceDesireStart});
	AddStep(table, k_Creature, StepKind::SetCreatureSpeedForJourney, {.made = true, .process = JourneyProcess});
}
