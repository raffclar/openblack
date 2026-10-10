/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/Components/Dance.h"
#include "ECS/DanceRules.h"

using namespace openblack;
using openblack::ecs::components::Dance;

TEST(DanceRules, TheRateGoesInStepsOfFourTenths)
{
	EXPECT_FLOAT_EQ(ecs::dance_rules::RateForSpeed(0.5f), 2.0f);
	EXPECT_FLOAT_EQ(ecs::dance_rules::RateForSpeed(0.25f), 0.8f);
	EXPECT_FLOAT_EQ(ecs::dance_rules::RateForSpeed(0.99f), 3.6f);
	EXPECT_FLOAT_EQ(ecs::dance_rules::RateForSpeed(1.0f), 4.0f);
}

TEST(DanceRules, AWorshipSitesDanceIsDancedWhileItsDancersChant)
{
	Dance dance;
	// As it is made: at a quarter speed, stopped
	ecs::dance_rules::SetSpeed(dance, 0.25f);
	EXPECT_EQ(dance.state, Dance::State::Stopped);
	EXPECT_FLOAT_EQ(dance.rate, 0.8f);
	EXPECT_FLOAT_EQ(dance.dancingRate, 1.0f);
	// Then set going at half speed: danced, starting over at the new rate
	dance.clock = 12.0f;
	ecs::dance_rules::SetWorshipSpeed(dance, 0.5f);
	EXPECT_EQ(dance.state, Dance::State::Dancing);
	EXPECT_FLOAT_EQ(dance.dancingRate, 2.0f);
	EXPECT_FLOAT_EQ(dance.clock, 0.0f);
	// The same rate again goes on where it was
	dance.clock = 5.0f;
	ecs::dance_rules::SetWorshipSpeed(dance, 0.55f);
	EXPECT_FLOAT_EQ(dance.clock, 5.0f);
	ecs::dance_rules::SetWorshipSpeed(dance, 0.0f);
	EXPECT_EQ(dance.state, Dance::State::Stopped);
}

TEST(DanceRules, ADanceStartsOnceMoreThanHalfOfThoseOnTheirWayHaveComeOrAfterTheLongestWait)
{
	Dance dance;
	EXPECT_FALSE(ecs::dance_rules::HasProperlyStarted(dance, 100));
	EXPECT_FALSE(dance.firstDancerTurn.has_value());
	dance.dancers = 2;
	dance.onTheirWay = 4;
	EXPECT_FALSE(ecs::dance_rules::HasProperlyStarted(dance, 100));
	EXPECT_EQ(dance.firstDancerTurn, 100u);
	dance.dancers = 3;
	EXPECT_TRUE(ecs::dance_rules::HasProperlyStarted(dance, 101));
	dance.dancers = 1;
	EXPECT_FALSE(ecs::dance_rules::HasProperlyStarted(dance, 100 + 899));
	EXPECT_TRUE(ecs::dance_rules::HasProperlyStarted(dance, 100 + 900));
}

TEST(DanceRules, TheClockRunsWhileItIsDancedAndStartsOverAfterItsLoop)
{
	Dance dance {.loopLength = 1};
	EXPECT_EQ(ecs::dance_rules::LoopTurns(dance), 600u);
	dance.dancers = 1;
	ecs::dance_rules::ProcessTurn(dance, true, 50);
	EXPECT_EQ(dance.state, Dance::State::Dancing);
	EXPECT_EQ(dance.startTurn, 50u);
	EXPECT_FLOAT_EQ(dance.clock, 1.0f);
	dance.clock = 598.0f;
	ecs::dance_rules::ProcessTurn(dance, true, 51);
	EXPECT_FLOAT_EQ(dance.clock, 599.0f);
	ecs::dance_rules::ProcessTurn(dance, true, 52);
	EXPECT_FLOAT_EQ(dance.clock, 0.0f);
	// No dancers, no clock
	dance.dancers = 0;
	ecs::dance_rules::ProcessTurn(dance, true, 53);
	EXPECT_FLOAT_EQ(dance.clock, 0.0f);
	// One that doesn't start by itself waits to be started
	Dance waiting {.loopLength = 1, .dancers = 5};
	ecs::dance_rules::ProcessTurn(waiting, false, 10);
	EXPECT_EQ(waiting.state, Dance::State::Stopped);
}
