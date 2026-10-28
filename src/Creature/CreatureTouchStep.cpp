/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureTouchStep.h"

#include <cmath>

#include <glm/common.hpp>
#include <glm/gtx/vec_swizzle.hpp>

using namespace openblack;
namespace feedback = openblack::creature_feedback;

bool creature_hand::WeighTouchStep(ecs::components::HandOnCreature& contact, const TouchStepInput& input,
                                   const TouchAnswers& creature)
{
	const bool onBody = input.onBody;
	const auto height = input.creatureHeight;
	auto& clocks = contact.clocks;
	clocks.sinceTouch += input.stepMs;
	clocks.ready += input.stepMs;
	clocks.sinceHold += input.stepMs;
	// How fast the hand moves across the land, up and down not counting: between the lines through the cursor now and
	// at the last step, both at the hand's distance
	// The game multiplies each part by one over the seconds before it finds the length
	const float perSecond = 1.0f / (input.stepMs / 1000.0f);
	const auto moved = (glm::xz(input.here) - glm::xz(input.before)) * perSecond;
	contact.speed = std::sqrt((moved.x * moved.x) + (moved.y * moved.y));
	const bool cursorMoved = input.cursor.x != contact.lastCursor.x;
	bool stroking = false;

	std::optional<feedback::Slap> slap;
	if (feedback::SlapConsidered(onBody, contact.wasOnBody, input.cursor.x, contact.lastCursor.x, input.screenWidth * 0.5f) &&
	    feedback::SlapReady(clocks))
	{
		slap = feedback::ClassifySlap(input.handHeight, contact.speed, height, input.cursor.x < contact.lastCursor.x);
	}
	if (slap.has_value())
	{
		if (feedback::CountSlap(contact.hardSlaps, *slap, onBody))
		{
			creature.bruise(feedback::BruiseColumn(contact.speed, height));
		}
		// The slap only counts when the creature can reel from it
		if (creature.slap(*slap))
		{
			feedback::Touched(clocks);
			contact.sum = feedback::AfterSlap(contact.sum, slap->gentle);
			contact.strokedOrSlapped = true;
		}
		clocks.contact = 0.0f;
	}
	else if (!onBody)
	{
		contact.lastPart.reset();
	}
	else
	{
		// Resting on the body long enough, the moving hand caresses it, and strokes the part it is nearest when one is due
		clocks.contact += input.stepMs;
		const auto part = clocks.contact > feedback::k_StrokeHoldMs && cursorMoved ? creature.nearestPart() : std::nullopt;
		if (part.has_value())
		{
			stroking = true;
			if (feedback::StrokeDue(clocks, contact.lastPart, *part, contact.sum) && creature.stroke(*part))
			{
				contact.lastPart = *part;
				feedback::Touched(clocks);
				contact.sum = feedback::AfterStroke(contact.sum);
				contact.strokedOrSlapped = true;
			}
		}
	}
	const bool fast = contact.speed > feedback::k_SlapSpeed * height;
	contact.cycle = feedback::NextCycle(contact.cycle, fast, stroking, onBody);
	contact.wasOnBody = onBody;
	contact.lastCursor = input.cursor;
	return slap.has_value();
}

glm::ivec2 creature_hand::StepCursor(glm::ivec2 before, glm::ivec2 now, const input_steps::Steps& steps, uint32_t step)
{
	return glm::ivec2(glm::round(input_steps::Between(glm::vec2(before), glm::vec2(now), steps.Point(step))));
}
