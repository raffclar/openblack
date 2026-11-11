/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureIdleMind.h"

using namespace openblack;
using creature_mind::ObjectOrder;
using creature_mind::Step;

namespace
{
constexpr float k_Turn = 0.1f;
constexpr uint32_t k_Rock = 7;
constexpr size_t k_MostTurns = 200;

creature_mind::Random Always(uint32_t value)
{
	return [value](uint32_t range) { return range == 0 ? 0u : std::min(value, range - 1); };
}

creature_mind::Senses IdleSenses()
{
	return {.seconds = k_Turn};
}

/// The turns of thought until the last step of the agenda ends, the turn that ends it included
size_t TurnsUntilLastStepEnds(creature_mind::IdleMind& mind, creature_mind::Commands& last)
{
	size_t turns = 0;
	while (mind.step < mind.agenda.size() && turns < k_MostTurns)
	{
		last = creature_mind::Think(mind, IdleSenses(), Always(0));
		++turns;
	}
	return turns;
}
} // namespace

TEST(CreatureStepEnds, AnAgendaWhoseLastStepHasNoEndStageEndsOnTheNextTurn)
{
	// A wait has no code in its end stage
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::BeIdle, {{.kind = Step::Kind::Wait, .seconds = 0.3f}});
	static_cast<void>(creature_mind::Think(mind, IdleSenses(), Always(0)));
	creature_mind::Commands last;
	ASSERT_LT(TurnsUntilLastStepEnds(mind, last), k_MostTurns);
	// The step is done, but the agenda stays until the next turn goes past the empty stage
	EXPECT_FALSE(creature_mind::AgendaOver(mind));
	EXPECT_FALSE(last.restEyes);
	const auto next = creature_mind::Think(mind, IdleSenses(), Always(0));
	EXPECT_TRUE(creature_mind::AgendaOver(mind));
	// The creature stops what it does on the turn that the agenda ends
	EXPECT_TRUE(next.restEyes);
}

TEST(CreatureStepEnds, AnAgendaWhoseLastStepHasAnEndStageEndsOnTheSameTurn)
{
	// A pose has code in its end stage: it waits there for the body to finish
	creature_mind::IdleMind mind;
	auto pose = Step {.kind = Step::Kind::Static, .seconds = 0.3f};
	pose.sequence = {1, 2, 3};
	creature_mind::Plan(mind, creature_mind::Activity::BeIdle, {pose});
	static_cast<void>(creature_mind::Think(mind, IdleSenses(), Always(0)));
	creature_mind::Commands last;
	ASSERT_LT(TurnsUntilLastStepEnds(mind, last), k_MostTurns);
	EXPECT_TRUE(creature_mind::AgendaOver(mind));
	EXPECT_TRUE(last.restEyes);
}

TEST(CreatureStepEnds, AStepInTheMiddleWithNoEndStageDoesNotHoldTheAgenda)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::BeIdle,
	                    {{.kind = Step::Kind::Wait, .seconds = 0.1f}, {.kind = Step::Kind::Wait, .seconds = 0.1f}});
	static_cast<void>(creature_mind::Think(mind, IdleSenses(), Always(0)));
	while (mind.step == 0)
	{
		static_cast<void>(creature_mind::Think(mind, IdleSenses(), Always(0)));
	}
	EXPECT_FALSE(mind.endsNextTurn.has_value());
	EXPECT_FALSE(creature_mind::AgendaOver(mind));
}

TEST(CreatureStepEnds, APickUpOfTheThingInTheHandEndsTheAgendaAtOnceAndSeesToNoDesire)
{
	// A pick-up has no code in its end stage, but a pick-up of the thing in the hand is not done at all
	creature_mind::IdleMind mind;
	auto pickUp = Step {.kind = Step::Kind::Object};
	pickUp.order = {.kind = ObjectOrder::Kind::PickUp, .object = k_Rock};
	creature_mind::Plan(mind, creature_mind::Activity::BeIdle, {pickUp});
	auto senses = IdleSenses();
	senses.held = k_Rock;
	static_cast<void>(creature_mind::Think(mind, senses, Always(0)));
	EXPECT_TRUE(creature_mind::AgendaOver(mind));
	EXPECT_EQ(mind.lastStepSkipped, mind.serial);
	// The head does not keep the look of a step that it did not do
	EXPECT_FALSE(mind.endedLook.has_value());
}

TEST(CreatureStepEnds, ANewAgendaClearsTheEndOfTheLastAgenda)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::BeIdle, {{.kind = Step::Kind::Wait, .seconds = 0.1f}});
	static_cast<void>(creature_mind::Think(mind, IdleSenses(), Always(0)));
	creature_mind::Commands last;
	ASSERT_LT(TurnsUntilLastStepEnds(mind, last), k_MostTurns);
	ASSERT_TRUE(mind.endsNextTurn.has_value());
	creature_mind::Plan(mind, creature_mind::Activity::BeIdle, {{.kind = Step::Kind::Wait, .seconds = 0.1f}});
	EXPECT_FALSE(mind.endsNextTurn.has_value());
	EXPECT_FALSE(mind.lastStepSkipped.has_value());
}

TEST(CreatureStepEnds, TheEyesOfALookChangeInEachTurnThatTheHeadTakesTheLook)
{
	creature_mind::IdleMind mind;
	auto flutter = Step {.kind = Step::Kind::Wait, .seconds = 0.1f, .flutterEyelids = true};
	creature_mind::Plan(mind, creature_mind::Activity::BeIdle, {flutter, {.kind = Step::Kind::Wait, .seconds = 0.3f}});
	// The look of the turn comes from the agenda as the last turn left it, before the step starts
	EXPECT_TRUE(creature_mind::TurnLookEyes(mind).flutterEyelids);
	while (mind.step == 0)
	{
		static_cast<void>(creature_mind::Think(mind, IdleSenses(), Always(0)));
	}
	// A wait has no code in its end stage, so its look holds for one more turn
	EXPECT_TRUE(creature_mind::TurnLookEyes(mind).flutterEyelids);
	static_cast<void>(creature_mind::Think(mind, IdleSenses(), Always(0)));
	EXPECT_FALSE(creature_mind::TurnLookEyes(mind).flutterEyelids);
}

TEST(CreatureStepEnds, ASleepyLookAndAWideLookComeFromTheStep)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::BeIdle,
	                    {{.kind = Step::Kind::Action, .animation = 1, .sleepyEyes = true},
	                     {.kind = Step::Kind::Action, .animation = 1, .heldEyes = creature_mind::Eyes::Wide}});
	EXPECT_EQ(creature_mind::TurnLookEyes(mind).eyes, creature_mind::Eyes::Sleepy);
	mind.step = 1;
	EXPECT_EQ(creature_mind::TurnLookEyes(mind).eyes, creature_mind::Eyes::Wide);
	mind.step = 2;
	EXPECT_EQ(creature_mind::TurnLookEyes(mind).eyes, creature_mind::Eyes::Unchanged);
	EXPECT_FALSE(creature_mind::TurnLookEyes(mind).flutterEyelids);
}
