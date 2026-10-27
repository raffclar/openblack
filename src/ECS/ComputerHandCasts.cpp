/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include "3D/MapCoords.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerHandSteps.h"

using namespace openblack;
using namespace openblack::computer_hand;
using namespace openblack::computer_hand::steps;

namespace
{

Result ChantStart(HandAction& action, const World& world, const Step& step)
{
	action.counter = 0;
	return world.casts.IconHasSite(ArgumentAt(step, 1).object) ? Result::Next : Result::Abort;
}

Result ChantProcess(HandAction& action, const World& world, const Step& step)
{
	const auto type = static_cast<MagicType>(ArgumentAt(step, 0).number);
	const auto icon = ArgumentAt(step, 1).object;
	++action.counter;
	if (icon == entt::null)
	{
		return Result::Abort;
	}
	// The worshippers already chant at the icon, or they start now
	if (world.casts.IconCharging(icon) || world.casts.StartChant(icon, type))
	{
		return Result::Next;
	}
	if (!world.casts.IconHasSite(icon))
	{
		return Result::Abort;
	}
	return WholeSeconds(action.counter) > k_ChantStartSeconds ? Result::Abort : Result::Continue;
}

Result ChantEnd(HandAction& action, const World& world, const Step& step)
{
	const auto icon = ArgumentAt(step, 1).object;
	if (icon == entt::null)
	{
		return Result::Abort;
	}
	if (world.casts.ChantNeeded(icon) <= 0.0f)
	{
		return Result::Next;
	}
	++action.counter;
	return WholeSeconds(action.counter) > k_ChantFinishSeconds ? Result::Abort : Result::Continue;
}

Result CastStart(HandAction& action, const World& /*world*/, const Step& step)
{
	action.castTarget = ArgumentAt(step, 0).object;
	return Result::Next;
}

Result CastProcess(HandAction& action, const World& world, const Step& step)
{
	const auto target = ArgumentAt(step, 0).object;
	const auto type = static_cast<MagicType>(ArgumentAt(step, 1).number);
	const auto icon = ArgumentAt(step, 2).object;
	// The hand stays over the thing, except for an angry miracle
	if (!world.casts.Angry(type))
	{
		KeepHandOver(action, world, target);
	}
	if (!world.casts.Cast(type, target, icon, action.hand))
	{
		return Result::Abort;
	}
	action.counter =
	    static_cast<uint32_t>(map_coords::FtoL(static_cast<float>(k_TurnsPerSecond) * world.casts.CastSeconds(type)));
	return Result::Next;
}

Result CastEnd(HandAction& action, const World& world, const Step& step)
{
	--action.counter;
	if (action.counter == 0)
	{
		return Result::Next;
	}
	KeepHandOver(action, world, ArgumentAt(step, 0).object);
	return Result::Continue;
}

/// The gesture that a step draws, from its magic type and its stage
uint32_t GestureOf(const World& world, const Step& step)
{
	return world.casts.SeedGesture(static_cast<MagicType>(ArgumentAt(step, 0).number), ArgumentAt(step, 1).number);
}

Result GestureStart(HandAction& action, const World& world, const Step& step)
{
	action.kept = action.hand;
	action.progress = 0.0f;
	action.hand = GestureHand(action.kept, world.casts.GesturePoint(GestureOf(world, step), 0.0f));
	return Result::Next;
}

Result GestureProcess(HandAction& action, const World& world, const Step& step)
{
	const auto gesture = GestureOf(world, step);
	const float share = GestureSharePerTurn(world.casts.GestureLength(gesture), world.casts.GestureLength(k_GestureCircle));
	action.progress = share + action.progress;
	if (!(action.progress < 1.0f))
	{
		action.progress = 1.0f;
		return Result::Next;
	}
	action.hand = GestureHand(action.kept, world.casts.GesturePoint(gesture, action.progress));
	return Result::Continue;
}

Result GestureEnd(HandAction& action, const World& world, const Step& step)
{
	// The hand goes back to where it started, and the gesture shows on the land there
	action.hand = action.kept;
	world.casts.ShowGesture(GestureOf(world, step), action.hand, k_GestureSize);
	return Result::Next;
}

} // namespace

void openblack::computer_hand::steps::AddCastSteps(StepTable& table)
{
	// CastSpellAtPos, CastSpellTowardPos, CastShieldAtPos, CloseDownCurrentSpell and WaitForSpellToWearOff are not made
	// yet
	AddStep(table, StepOwner::Casts, StepKind::MakeWorshippersChantForSpell,
	        {.made = true, .start = ChantStart, .process = ChantProcess, .end = ChantEnd});
	AddStep(table, StepOwner::Casts, StepKind::MakeWorshippersChantForSpellNoWait,
	        {.made = true, .start = ChantStart, .process = ChantProcess});
	AddStep(table, StepOwner::Casts, StepKind::CastSpellAtObject,
	        {.made = true, .start = CastStart, .process = CastProcess, .end = CastEnd});
	AddStep(table, StepOwner::Casts, StepKind::MakeGesture,
	        {.made = true, .start = GestureStart, .process = GestureProcess, .end = GestureEnd});
}
