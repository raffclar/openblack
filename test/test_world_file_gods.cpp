/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/WorldFileGods.h"

using namespace openblack;
using ecs::world_file_gods::Change;
using ecs::world_file_gods::ChangeOf;
using ecs::world_file_gods::God;

TEST(WorldFileGods, TheGodOfAPlayerOfTheRoomGetsHandsAndNoComputerMind)
{
	// A god of the file with hands and no mind stays as it is
	EXPECT_EQ(ChangeOf({.player = PlayerNames::PLAYER_TWO, .roomPlayer = true, .hands = true, .computerMind = false}),
	          Change {});
	// A god of the file with no hands gets hands
	EXPECT_EQ(ChangeOf({.player = PlayerNames::PLAYER_TWO, .roomPlayer = true, .hands = false, .computerMind = false}),
	          (Change {.makeHands = true, .mindOff = false, .removeHands = false}));
	// A computer god of the file becomes the god of the human
	EXPECT_EQ(ChangeOf({.player = PlayerNames::PLAYER_THREE, .roomPlayer = true, .hands = false, .computerMind = true}),
	          (Change {.makeHands = true, .mindOff = true, .removeHands = false}));
	EXPECT_EQ(ChangeOf({.player = PlayerNames::PLAYER_ONE, .roomPlayer = true, .hands = true, .computerMind = true}),
	          (Change {.makeHands = false, .mindOff = true, .removeHands = false}));
}

TEST(WorldFileGods, AGodWithNoPlayerOfTheRoomIsAsASlotWithNoPlayer)
{
	// The human of the file is not in the room: the god loses its hands, and a computer god plays it with no new creature
	// because it has no citadel
	EXPECT_EQ(ChangeOf({.player = PlayerNames::PLAYER_THREE, .roomPlayer = false, .hands = true, .computerMind = false}),
	          (Change {.removeHands = true, .computerGod = true, .makeCreature = false}));
	// A computer god keeps its mind, and a god with no hands stays as it is
	EXPECT_EQ(ChangeOf({.player = PlayerNames::PLAYER_FOUR, .roomPlayer = false, .hands = false, .computerMind = true}),
	          Change {});
	EXPECT_EQ(ChangeOf({.player = PlayerNames::PLAYER_FIVE, .roomPlayer = false, .hands = false, .computerMind = false}),
	          Change {});
	// The first player always keeps its hands, as at the start of a network game
	EXPECT_EQ(ChangeOf({.player = PlayerNames::PLAYER_ONE, .roomPlayer = false, .hands = true, .computerMind = false}),
	          (Change {.computerGod = true}));
}

TEST(WorldFileGods, AGodWithACitadelAndNoPlayerOfTheRoomGetsAComputerGod)
{
	// The human of the file is not in the room: the god loses its hands and gets a computer god with a new creature
	EXPECT_EQ(ChangeOf({.player = PlayerNames::PLAYER_THREE, .roomPlayer = false, .hands = true, .citadel = true}),
	          (Change {.removeHands = true, .computerGod = true, .makeCreature = true}));
	// A god with a creature keeps it: the computer god gets no second creature
	EXPECT_EQ(
	    ChangeOf({.player = PlayerNames::PLAYER_THREE, .roomPlayer = false, .hands = true, .citadel = true, .creature = true}),
	    (Change {.removeHands = true, .computerGod = true, .makeCreature = false}));
	// A slot with no hands and no mind fills as at the start of a network game
	EXPECT_EQ(ChangeOf({.player = PlayerNames::PLAYER_FIVE, .roomPlayer = false, .citadel = true}),
	          (Change {.computerGod = true, .makeCreature = true}));
	// The first player keeps its hands and also gets a computer god
	EXPECT_EQ(ChangeOf({.player = PlayerNames::PLAYER_ONE, .roomPlayer = false, .hands = true, .citadel = true}),
	          (Change {.computerGod = true, .makeCreature = true}));
}

TEST(WorldFileGods, AComputerGodOrAGodWithNoCitadelGetsNoNewComputerGod)
{
	EXPECT_EQ(ChangeOf({.player = PlayerNames::PLAYER_FOUR, .roomPlayer = false, .computerMind = true, .citadel = true}),
	          Change {});
	EXPECT_EQ(ChangeOf({.player = PlayerNames::PLAYER_FOUR, .roomPlayer = false, .citadel = false}), Change {});
	// A player of the room never gets a computer god
	EXPECT_EQ(ChangeOf({.player = PlayerNames::PLAYER_TWO, .roomPlayer = true, .hands = true, .citadel = true}), Change {});
}
