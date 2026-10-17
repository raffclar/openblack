/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <gtest/gtest.h>

#include "Animals/AnimalRules.h"
#include "InfoConstants.h"

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

TEST(AnimalReactions, AfterAReactionAnAnimalGoesIntoTheResumeStateOfItsStateBefore)
{
	// A small table: a script state with the empty state, and a wander state that decides again
	std::array<GAnimalStateTableInfo, 33> table {};
	table.at(4).resumeState = 0;
	table.at(32).resumeState = 43;
	EXPECT_EQ(animals::StateAfterReacting(32, table), 43);
	EXPECT_EQ(animals::StateAfterReacting(4, table), 0);
}

TEST(AnimalReactions, AStateThatIsNotInTheTableGivesTheEmptyState)
{
	std::array<GAnimalStateTableInfo, 4> table {};
	table.at(3).resumeState = 43;
	EXPECT_EQ(animals::StateAfterReacting(4, table), 0);
}

TEST(AnimalReactions, BatsVulturesAndTheBigHuntersFrightenACreature)
{
	for (const auto type : {AnimalInfo::Bat, AnimalInfo::SpellBat, AnimalInfo::Vulture, AnimalInfo::Lion, AnimalInfo::Tiger,
	                        AnimalInfo::Leopard, AnimalInfo::Wolf, AnimalInfo::SpellWolf})
	{
		EXPECT_TRUE(animals::FrightensCreatures(type)) << static_cast<int>(type);
	}
	// The lion and the cow of the puzzles and the farm animals do not frighten a creature
	for (const auto type :
	     {AnimalInfo::PuzzleLion, AnimalInfo::PuzzleCow, AnimalInfo::Sheep, AnimalInfo::Cow, AnimalInfo::Horse})
	{
		EXPECT_FALSE(animals::FrightensCreatures(type)) << static_cast<int>(type);
	}
}
