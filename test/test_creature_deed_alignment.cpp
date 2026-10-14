/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Creature/CreatureDeedAlignment.h"

using namespace openblack::creature_deed_alignment;

TEST(CreatureDeedAlignment, TheDesireOfThePlanGivesTheChange)
{
	// A neutral creature takes the change of the desire, multiplied by the species scale
	EXPECT_FLOAT_EQ(TurnChange({.desireChange = 1.0f}, 0.2f, 0.0f), 0.2f);
	EXPECT_FLOAT_EQ(TurnChange({.desireChange = -0.2f}, 0.2f, -0.0f), -0.04f);
	EXPECT_FLOAT_EQ(TurnChange({}, 0.2f, 0.5f), 0.0f);
}

TEST(CreatureDeedAlignment, AnEatenVillagerReplacesTheChangeOfTheDesire)
{
	EXPECT_FLOAT_EQ(TurnChange({.desireChange = 0.3f, .eatenVillagerWeight = -1.0f}, 0.2f, 0.0f), -0.2f);
	// The weight of a skeleton is good
	EXPECT_FLOAT_EQ(TurnChange({.eatenVillagerWeight = 1.0f}, 0.2f, 0.0f), 0.2f);
}

TEST(CreatureDeedAlignment, TheAlignmentOfTheCreatureChangesTheChange)
{
	// A change in the direction of the alignment becomes smaller by half the alignment
	EXPECT_FLOAT_EQ(TurnChange({.desireChange = 1.0f}, 1.0f, 0.5f), 0.75f);
	EXPECT_FLOAT_EQ(TurnChange({.eatenVillagerWeight = -1.0f}, 1.0f, -0.5f), -0.75f);
	// A change against the alignment becomes larger by half the alignment
	EXPECT_FLOAT_EQ(TurnChange({.desireChange = 1.0f}, 1.0f, -0.5f), 1.25f);
	EXPECT_FLOAT_EQ(TurnChange({.eatenVillagerWeight = -1.0f}, 1.0f, 0.5f), -1.25f);
}
