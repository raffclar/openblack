/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <gtest/gtest.h>

#include "Creature/CreatureOwner.h"

using namespace openblack;

TEST(CreatureOwner, AThrowOfACreatureOfNoPlayerGivesTheCreditToNoPlayer)
{
	// The guide, the Ogre and a creature that a script makes belong to no player
	EXPECT_FALSE(creature_owner::CreditedPlayer(PlayerNames::NEUTRAL).has_value());
	// The player's creature gives the credit to its player
	EXPECT_EQ(creature_owner::CreditedPlayer(PlayerNames::PLAYER_ONE), std::optional(PlayerNames::PLAYER_ONE));
	EXPECT_EQ(creature_owner::CreditedPlayer(PlayerNames::PLAYER_TWO), std::optional(PlayerNames::PLAYER_TWO));
}
