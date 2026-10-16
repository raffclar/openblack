/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFollowUp.h"

using namespace openblack;
using namespace openblack::creature_follow_up;
using creature_mind::Step;

namespace
{
Step Clip(size_t animation)
{
	return {.kind = Step::Kind::Action, .animation = animation};
}
} // namespace

std::optional<FollowUp> creature_follow_up::AfterMushroom(std::optional<FollowUp> waiting)
{
	return waiting.has_value() ? waiting : std::optional(FollowUp::BecomeHigh);
}

std::optional<FollowUp> creature_follow_up::AfterYoungFeedback(std::optional<FollowUp> waiting, float feedback, uint32_t roll)
{
	if (roll != 0)
	{
		return waiting;
	}
	return feedback >= 0.0f ? FollowUp::ShowHappy : FollowUp::FrightenedOfPlayer;
}

std::string_view creature_follow_up::FrightenedAction(uint32_t roll)
{
	return roll != 0 ? "BeFrightenedOnTheSpot" : "RunAwayFromPlayer";
}

std::vector<Step> creature_follow_up::ShowHappy()
{
	return {Clip(k_Happy)};
}

std::vector<Step> creature_follow_up::ShowSlapFeeling(const Feelings& feelings, const std::function<uint32_t(uint32_t)>& random,
                                                      float chance, uint32_t turnsPerSecond)
{
	switch (random(4))
	{
	case 0:
		if (feelings.compassion >= feelings.anger)
		{
			return {Clip(k_Frightened)};
		}
		return {Clip(random(2) == 0 ? k_Angry : k_RudeGesture)};
	case 1:
		return {Clip(k_Sad)};
	case 2:
		return {Clip(k_Cry)};
	default:
	{
		const auto turns = static_cast<uint32_t>((chance + 1.0f) * static_cast<float>(turnsPerSecond));
		return {{.kind = Step::Kind::WatchCamera,
		         .turns = turns,
		         .face = creature_face::Cue::Sad,
		         .heldEyes = creature_mind::Eyes::Stoned}};
	}
	}
}

std::string_view creature_follow_up::ActionToDoAgain(std::string_view stroked)
{
	return stroked == "EatObjectInHand" ? std::string_view("EatAlive") : stroked;
}

bool creature_follow_up::MayDoAgain(std::optional<uint32_t> actedOn, uint32_t self, bool copying)
{
	return actedOn.has_value() && *actedOn != self && !copying;
}

std::optional<uint32_t> creature_follow_up::ThingToDoAgainTo(bool actsOnThing, uint32_t self,
                                                             std::span<const creature_planner::ObjectCandidate> sameKind)
{
	if (!actsOnThing)
	{
		return self;
	}
	std::optional<uint32_t> best;
	float bestUsefulness = 0.0f;
	for (const auto& thing : sameKind)
	{
		if (thing.usefulness > bestUsefulness)
		{
			best = thing.id;
			bestUsefulness = thing.usefulness;
		}
	}
	return best;
}

bool creature_follow_up::SuitsDoAgain(const DoAgainThing& thing)
{
	return thing.sameKind && thing.passesTest.value_or(true) && !(thing.notWhatItHolds && thing.heldByItself);
}
