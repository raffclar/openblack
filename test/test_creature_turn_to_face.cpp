/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

#include "Creature/CreatureTurnToFace.h"

using namespace openblack;
namespace turn_to_face = openblack::creature_turn_to_face;
using Phase = turn_to_face::Turn::Phase;

namespace
{
/// A body that starts a turn only when it is free, and counts the turns that it was asked for
struct FakeBody
{
	bool free {true};
	size_t asked {0};
	bool Start()
	{
		++asked;
		return free;
	}
};

void StepWith(turn_to_face::Turn& turn, const turn_to_face::Senses& senses, FakeBody& body, uint32_t hold = 1)
{
	turn_to_face::Step(turn, senses, hold, [&body] { return body.Start(); });
}
} // namespace

// A tenth of a second at ten turns a second is one turn
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureTurnToFaceTest, TheHoldIsTheSecondsInTurnsCutOff)
{
	EXPECT_EQ(turn_to_face::HoldTurns(0.1f, 10), 1u);
	EXPECT_EQ(turn_to_face::HoldTurns(1.4f, 10), 14u);
	EXPECT_EQ(turn_to_face::HoldTurns(0.05f, 10), 0u);
}

// A body that plays an action refuses the turn, and the creature asks again on each turn until the body takes it
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureTurnToFaceTest, TheTurnWaitsForTheBodyThatPlaysAnAction)
{
	turn_to_face::Turn turn {.phase = Phase::Start};
	FakeBody body {.free = false};
	StepWith(turn, {.bodyBusy = true}, body);
	StepWith(turn, {.bodyBusy = true}, body);
	EXPECT_EQ(turn.phase, Phase::Start);
	EXPECT_EQ(body.asked, 2u);
	body.free = true;
	StepWith(turn, {}, body);
	EXPECT_EQ(turn.phase, Phase::Check);
	EXPECT_EQ(body.asked, 3u);
}

// After the turn starts, it waits while the body turns, checks that it faces the thing, holds, and is done
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureTurnToFaceTest, ItWaitsForTheTurnThenHoldsAndIsDone)
{
	turn_to_face::Turn turn {.phase = Phase::Start};
	FakeBody body;
	StepWith(turn, {}, body);
	ASSERT_EQ(turn.phase, Phase::Check);
	// The body turns
	StepWith(turn, {.bodyBusy = true}, body);
	EXPECT_EQ(turn.phase, Phase::Check);
	// It faces the thing and holds for one turn
	StepWith(turn, {.facing = true}, body);
	EXPECT_EQ(turn.phase, Phase::Hold);
	EXPECT_EQ(turn.holdTurns, 1u);
	StepWith(turn, {.facing = true}, body);
	EXPECT_EQ(turn.phase, Phase::Hold);
	EXPECT_EQ(turn.holdTurns, 0u);
	StepWith(turn, {.facing = true}, body);
	EXPECT_EQ(turn.phase, Phase::Done);
	EXPECT_EQ(body.asked, 1u);
}

// A creature that does not face the thing after its turn, or that the thing moved round, turns again
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureTurnToFaceTest, ItTurnsAgainWhenItDoesNotFaceTheThing)
{
	turn_to_face::Turn turn {.phase = Phase::Check};
	FakeBody body;
	StepWith(turn, {}, body);
	EXPECT_EQ(turn.phase, Phase::Check);
	EXPECT_EQ(body.asked, 1u);
	StepWith(turn, {.facing = true}, body, 2);
	ASSERT_EQ(turn.phase, Phase::Hold);
	StepWith(turn, {}, body, 2);
	EXPECT_EQ(turn.phase, Phase::Hold);
	EXPECT_EQ(turn.holdTurns, 1u);
	EXPECT_EQ(body.asked, 2u);
}

// Right on top of the thing, it does not turn and is done after the next turns
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureTurnToFaceTest, OnTopOfTheThingItDoesNotTurn)
{
	turn_to_face::Turn turn {.phase = Phase::Start};
	FakeBody body;
	StepWith(turn, {.onTop = true}, body);
	EXPECT_EQ(turn.phase, Phase::Check);
	StepWith(turn, {.onTop = true}, body);
	EXPECT_EQ(turn.phase, Phase::Hold);
	StepWith(turn, {.onTop = true}, body);
	EXPECT_EQ(turn.phase, Phase::Done);
	EXPECT_EQ(body.asked, 0u);
}

// A turn that it was not told to do, or that is done, does nothing
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureTurnToFaceTest, AnIdleOrDoneTurnDoesNothing)
{
	FakeBody body;
	turn_to_face::Turn idle {};
	StepWith(idle, {}, body);
	EXPECT_EQ(idle.phase, Phase::Idle);
	turn_to_face::Turn done {.phase = Phase::Done};
	StepWith(done, {}, body);
	EXPECT_EQ(done.phase, Phase::Done);
	EXPECT_EQ(body.asked, 0u);
}
