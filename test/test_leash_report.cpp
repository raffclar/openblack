/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Creature/LeashReport.h"

using namespace openblack;
using namespace openblack::creature_leash;

TEST(LeashReport, PlayerWithNoCreature)
{
	const CommandReport report {.player = 1, .command = "presses the leash key", .turn = 40};
	EXPECT_EQ(ReportLine(report), "Leash command: player 1 presses the leash key at turn 40: the player has no creature");
}

TEST(LeashReport, CreatureWithNoLeash)
{
	const CommandReport report {.player = 0, .command = "goes into the temple", .turn = 812, .creature = 3051};
	EXPECT_EQ(ReportLine(report), "Leash command: player 0 goes into the temple at turn 812: creature 3051 wears no leash");
}

TEST(LeashReport, LeashInTheHand)
{
	const CommandReport report {
	    .player = 2,
	    .command = "presses the leash key",
	    .turn = 500,
	    .creature = 77,
	    .worn = WornReport {.type = LeashType::Rope, .holder = 2},
	};
	EXPECT_EQ(ReportLine(report),
	          "Leash command: player 2 presses the leash key at turn 500: creature 77 wears leash 2 of player 2, held in "
	          "the hand");
}

TEST(LeashReport, LeashTiedToAThing)
{
	const CommandReport report {
	    .player = 0,
	    .command = "gives a leash order",
	    .turn = 9,
	    .creature = 5,
	    .worn = WornReport {.type = LeashType::Good, .holder = 0, .tiedTo = 1200},
	};
	EXPECT_EQ(
	    ReportLine(report),
	    "Leash command: player 0 gives a leash order at turn 9: creature 5 wears leash 3 of player 0, tied to thing 1200");
}
