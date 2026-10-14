/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Animals/AnimalRules.h"

using namespace openblack;

TEST(AnimalReactions, AFreeAnimalTakesUpReactions)
{
	EXPECT_TRUE(animals::AvailableForReaction({}));
}

TEST(AnimalReactions, AnAnimalAScriptControlsTakesUpNone)
{
	// A script's cow stays where it was put, even when a creature walks up to it
	EXPECT_FALSE(animals::AvailableForReaction({.scriptControlled = true}));
}

TEST(AnimalReactions, AnAnimalOutOfTheWorldOrBusyTakesUpNone)
{
	EXPECT_FALSE(animals::AvailableForReaction({.outOfTheWorld = true}));
	EXPECT_FALSE(animals::AvailableForReaction({.busyState = true}));
}

TEST(AnimalReactions, BusyStatesAreTheGamesDancingDyingDeadDrowningDownedEatenAndWaitingForAClip)
{
	for (uint8_t state = 0; state < 60; ++state)
	{
		const bool busy = state == 5 || (state >= 14 && state <= 18) || state == 23;
		EXPECT_EQ(animals::IsBusyForReaction(state), busy) << "state " << static_cast<int>(state);
	}
	// Setting off to die is not yet dying
	EXPECT_FALSE(animals::IsBusyForReaction(13));
}
