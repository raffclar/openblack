/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Creature/CreatureEnvironmentMap.h"
#include "Graphics/MaterialStages.h"

using namespace openblack::creature_environment_map;
using openblack::graphics::material_stages::EnvironmentStage;

TEST(CreatureEnvironmentMap, AValueThatIsNotZeroDrawsTheBodyWithTheMapAddedOverEveryPart)
{
	EXPECT_FALSE(DrawnWithMap(0));
	EXPECT_TRUE(DrawnWithMap(1));
	EXPECT_TRUE(DrawnWithMap(0xFFFFFFFFu));
	EXPECT_EQ(openblack::graphics::material_stages::k_CreatureBodyStage, EnvironmentStage::Added);
}

TEST(CreatureEnvironmentMap, TheBodyWithTheMapNeverFizzesAndShowsWhole)
{
	// Without the map, the body fizzes and goes out of sight at the end
	EXPECT_TRUE(BodyFizzes(0.5f, 0));
	EXPECT_TRUE(BodyDrawn(0.5f, 0));
	EXPECT_FALSE(BodyDrawn(1.0f, 0));
	// With the map, the body shows whole however far it has fizzed
	EXPECT_FALSE(BodyFizzes(0.5f, 1));
	EXPECT_TRUE(BodyDrawn(0.5f, 1));
	EXPECT_TRUE(BodyDrawn(1.0f, 1));
	EXPECT_FALSE(BodyFizzes(0.0f, 1));
}

TEST(CreatureEnvironmentMap, TheBodyWithTheMapTakesNoFrozenLook)
{
	EXPECT_TRUE(FrozenLookShown(0));
	EXPECT_FALSE(FrozenLookShown(7));
}

TEST(CreatureEnvironmentMap, TheHairOfABodyWithTheMapShowsAtAllTimes)
{
	EXPECT_TRUE(HairShown(0.0f, 0.0f, 0));
	EXPECT_FALSE(HairShown(0.5f, 0.0f, 0));
	EXPECT_FALSE(HairShown(0.0f, 0.5f, 0));
	EXPECT_TRUE(HairShown(0.5f, 0.0f, 1));
	EXPECT_TRUE(HairShown(1.0f, 1.0f, 1));
}
