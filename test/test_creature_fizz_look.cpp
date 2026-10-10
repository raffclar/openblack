/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Creature/CreatureFizzLook.h"

using namespace openblack::creature_fizz_look;

TEST(CreatureFizzLook, TheLevelDropsTheFraction)
{
	EXPECT_EQ(Level(0.0f), 0);
	EXPECT_EQ(Level(0.5f), 127);
	EXPECT_EQ(Level(0.999f), 254);
	EXPECT_EQ(Level(1.0f), 255);
	// Kept within 0 to 1
	EXPECT_EQ(Level(-0.5f), 0);
	EXPECT_EQ(Level(2.0f), 255);
}

TEST(CreatureFizzLook, NothingIsDrawnRightOutOfSight)
{
	EXPECT_TRUE(Drawn(0.0f));
	EXPECT_TRUE(Drawn(0.999f));
	EXPECT_FALSE(Drawn(1.0f));
	EXPECT_FALSE(Fizzing(0.0f));
	EXPECT_TRUE(Fizzing(0.001f));
	EXPECT_TRUE(Fizzing(0.999f));
	EXPECT_FALSE(Fizzing(1.0f));
}

TEST(CreatureFizzLook, TheStaticMustReachFiveUnderTheLevel)
{
	EXPECT_EQ(StaticThreshold(0), 0);
	EXPECT_EQ(StaticThreshold(5), 0);
	EXPECT_EQ(StaticThreshold(6), 1);
	EXPECT_EQ(StaticThreshold(254), 249);
}

TEST(CreatureFizzLook, TheBodyBlendsByWhatTheLevelLeaves)
{
	EXPECT_FLOAT_EQ(BodyAlpha(0), 1.0f);
	EXPECT_FLOAT_EQ(BodyAlpha(127), 128.0f / 255.0f);
	EXPECT_FLOAT_EQ(BodyAlpha(254), 1.0f / 255.0f);
}

TEST(CreatureFizzLook, TheStaticScrollsAndWraps)
{
	const auto once = Scroll({0.0f, 0.0f}, 1.0f);
	EXPECT_NEAR(once.x, 0.1f, 1e-6f);
	EXPECT_NEAR(once.y, 0.2f, 1e-6f);
	const auto wrapped = Scroll({0.95f, 0.9f}, 1.0f);
	EXPECT_NEAR(wrapped.x, 0.05f, 1e-6f);
	EXPECT_NEAR(wrapped.y, 0.1f, 1e-6f);
	// No time, no slide
	EXPECT_EQ(Scroll({0.3f, 0.4f}, 0.0f), glm::vec2(0.3f, 0.4f));
}

TEST(CreatureFizzLook, HairAndReflectionGoAFifthOfTheWayOut)
{
	EXPECT_TRUE(HairShown(0.0f, 0.0f));
	EXPECT_TRUE(HairShown(0.19f, 0.0f));
	EXPECT_FALSE(HairShown(0.2f, 0.0f));
	EXPECT_FALSE(HairShown(1.0f, 0.0f));
	EXPECT_TRUE(ReflectionShown(0.0f));
	EXPECT_TRUE(ReflectionShown(0.19f));
	EXPECT_FALSE(ReflectionShown(0.2f));
}

TEST(CreatureFizzLook, FreezeHidesTheHairAFifthOfTheWay)
{
	EXPECT_TRUE(HairShown(0.0f, 0.19f));
	EXPECT_FALSE(HairShown(0.0f, 0.2f));
	EXPECT_FALSE(HairShown(0.1f, 1.0f));
	// The reflection goes with the fizz alone
	EXPECT_TRUE(ReflectionShown(0.0f));
}

TEST(CreatureFizzLook, FrozenEyesAreNotFizzed)
{
	EXPECT_TRUE(EyesFizz(0.5f, 0.0f));
	EXPECT_FALSE(EyesFizz(0.5f, 0.01f));
	EXPECT_FALSE(EyesFizz(0.0f, 0.0f));
	// Wholly out of sight, nothing is drawn through the static
	EXPECT_FALSE(EyesFizz(1.0f, 0.0f));
}

TEST(CreatureFizzLook, EyelidsGoWhiteOnlyWhenWhollyFrozen)
{
	EXPECT_FALSE(EyelidsWhite(0.0f));
	EXPECT_FALSE(EyelidsWhite(0.999f));
	EXPECT_TRUE(EyelidsWhite(1.0f));
}
