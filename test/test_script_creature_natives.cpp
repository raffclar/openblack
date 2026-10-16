/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/LeashRules.h"
#include "ScriptHeaders/ScriptCreatureRules.h"

using namespace openblack;
using namespace openblack::script::creature_rules;

TEST(ScriptCreatureNatives, LeashTypeIsThePickedTemplePost)
{
	const std::array posts {LeashPostState {.type = LeashType::Evil, .selected = false},
	                        LeashPostState {.type = LeashType::Rope, .selected = true},
	                        LeashPostState {.type = LeashType::Good, .selected = false}};
	EXPECT_EQ(LeashTypeForScripts(posts), 2);
}

TEST(ScriptCreatureNatives, LeashTypeWithNonePickedIsMinusOne)
{
	const std::array posts {LeashPostState {.type = LeashType::Evil, .selected = false},
	                        LeashPostState {.type = LeashType::Rope, .selected = false}};
	EXPECT_EQ(LeashTypeForScripts(posts), -1);
}

TEST(ScriptCreatureNatives, LeashTypeWithNoTempleLeashesIsZero)
{
	EXPECT_EQ(LeashTypeForScripts({}), 0);
}

TEST(ScriptCreatureNatives, EitherThingMayBeTheLeashedCreature)
{
	EXPECT_EQ(WhichIsLeashed(true, false), LeashedPair::FirstIsCreature);
	EXPECT_EQ(WhichIsLeashed(true, true), LeashedPair::FirstIsCreature);
	EXPECT_EQ(WhichIsLeashed(false, true), LeashedPair::SecondIsCreature);
	EXPECT_EQ(WhichIsLeashed(false, false), LeashedPair::Neither);
}

TEST(ScriptCreatureNatives, ConfinementIsOnTheGroundAsPreciseAsAMapPosition)
{
	const auto area = ConfinementFor({100.0f, 37.5f, 200.0f}, 25.0f);
	EXPECT_FLOAT_EQ(area.centre.y, 0.0f);
	EXPECT_FLOAT_EQ(area.radius, 25.0f);
	// A tenth of a millimetre is less than a map unit: the centre keeps the whole units only
	const auto fine = ConfinementFor({10.00001f, 0.0f, 0.0f}, 1.0f);
	EXPECT_NEAR(fine.centre.x, 10.0f, 2e-4f);
	EXPECT_LE(fine.centre.x, 10.00001f);
}

TEST(ScriptCreatureNatives, OnlyTheLearningLeashKeepsTheDominantDesireWhenTakenOff)
{
	EXPECT_FALSE(creature_leash::TakingOffEndsDominantDesire(LeashType::Rope));
	EXPECT_TRUE(creature_leash::TakingOffEndsDominantDesire(LeashType::Evil));
	EXPECT_TRUE(creature_leash::TakingOffEndsDominantDesire(LeashType::Good));
}
