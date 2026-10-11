/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <numbers>

#include <gtest/gtest.h>

#include "ECS/TownDesireFlags.h"
#include "Enums.h"

namespace flags = openblack::ecs::town_desire_flags;
using openblack::TownDesireInfo;

namespace
{
constexpr float k_ShowsAfter = 0.1f;
} // namespace

TEST(TownDesireFlags, AFlagFliesAtTheStoresPointAfterTheFirstTwelve)
{
	EXPECT_EQ(flags::PointOf(0), 12u);
	EXPECT_EQ(flags::PointOf(5), 17u);
	EXPECT_FALSE(flags::PointOf(-1).has_value());
}

TEST(TownDesireFlags, AFlagShowsOnlyOverAWorkingStoreOnceItIsUpFarEnough)
{
	EXPECT_TRUE(flags::Shows(true, 0.1f, k_ShowsAfter));
	EXPECT_TRUE(flags::Shows(true, 0.5f, k_ShowsAfter));
	EXPECT_FALSE(flags::Shows(true, 0.09f, k_ShowsAfter));
	EXPECT_FALSE(flags::Shows(false, 0.5f, k_ShowsAfter));
}

TEST(TownDesireFlags, AHiddenFlagIsPutStraightAtItsDesireEvenPastWhole)
{
	const auto next = flags::Step({.position = 0.3f, .speed = 0.7f}, false, 1.6f, 0.016f);
	EXPECT_FLOAT_EQ(next.position, 1.6f);
	EXPECT_FLOAT_EQ(next.speed, 0.0f);
}

TEST(TownDesireFlags, AShownFlagEasesUpToItsDesireNeverPastWhole)
{
	openblack::field_crop::Settle level {.position = 0.2f, .speed = 0.0f};
	// A step of a frame moves it a little only, setting off from rest
	const auto first = flags::Step(level, true, 0.8f, 0.02f);
	EXPECT_GT(first.position, 0.2f);
	EXPECT_LT(first.position, 0.21f);
	EXPECT_GT(first.speed, 0.0f);
	// Frame after frame it comes to rest at its desire, held to wholly wanted
	for (int i = 0; i < 1000; ++i)
	{
		level = flags::Step(level, true, 3.0f, 0.02f);
	}
	EXPECT_NEAR(level.position, 1.0f, 1e-4f);
	EXPECT_NEAR(level.speed, 0.0f, 1e-3f);
	// A frame of a second or more puts it there at once
	const auto jump = flags::Step({.position = 0.2f, .speed = 0.0f}, true, 0.6f, 1.0f);
	EXPECT_FLOAT_EQ(jump.position, 0.6f);
	EXPECT_FLOAT_EQ(jump.speed, 0.0f);
}

TEST(TownDesireFlags, AFlagRisesFourMetresAndTwoFifthsWhenWhollyWanted)
{
	EXPECT_FLOAT_EQ(flags::Rise(0.0f), 0.0f);
	EXPECT_FLOAT_EQ(flags::Rise(1.0f), 4.4f);
	EXPECT_FLOAT_EQ(flags::Rise(0.5f), 2.2f);
}

TEST(TownDesireFlags, AFlagGrowsFromNothingAsItFirstShows)
{
	// Its whole size from a fifth of the room above its first showing: 0.18 for a desire that shows from 0.1
	EXPECT_FLOAT_EQ(flags::DrawnScale(0.1f, k_ShowsAfter), 0.0f);
	EXPECT_NEAR(flags::DrawnScale(0.14f, k_ShowsAfter), 0.5f, 1e-5f);
	EXPECT_FLOAT_EQ(flags::DrawnScale(0.18f, k_ShowsAfter), 1.0f);
	EXPECT_FLOAT_EQ(flags::DrawnScale(0.9f, k_ShowsAfter), 1.0f);
}

TEST(TownDesireFlags, AFlagTurnsAQuarterPastTheWayToTheCamera)
{
	const glm::vec3 flag {10.0f, 0.0f, 10.0f};
	EXPECT_NEAR(flags::FacingAngle(flag, {20.0f, 5.0f, 10.0f}), std::numbers::pi_v<float> / 2.0f, 1e-6f);
	EXPECT_NEAR(flags::FacingAngle(flag, {10.0f, 5.0f, 20.0f}), std::numbers::pi_v<float>, 1e-6f);
}

TEST(TownDesireFlags, OnlyPointsInsideTheViewAreInView)
{
	EXPECT_TRUE(flags::InView({0.5f, -0.5f, 0.5f, 1.0f}));
	EXPECT_FALSE(flags::InView({1.5f, 0.0f, 0.5f, 1.0f}));
	EXPECT_FALSE(flags::InView({0.0f, 0.0f, 0.5f, -1.0f}));
}

TEST(TownDesireFlags, EachFlagShowsItsOwnWordsOverTheNeed)
{
	EXPECT_EQ(flags::ToolTipWords(TownDesireInfo::ForFood), 91u);
	EXPECT_EQ(flags::ToolTipWords(TownDesireInfo::ForWood), 92u);
	EXPECT_EQ(flags::ToolTipWords(TownDesireInfo::ForProtection), 94u);
	EXPECT_EQ(flags::ToolTipWords(TownDesireInfo::ForMercy), 95u);
	EXPECT_EQ(flags::ToolTipWords(TownDesireInfo::ForAbodes), 96u);
	EXPECT_EQ(flags::ToolTipWords(TownDesireInfo::ForCivicBuilding), 97u);
	EXPECT_EQ(flags::ToolTipWords(TownDesireInfo::ForChildren), 99u);
	EXPECT_FALSE(flags::ToolTipWords(TownDesireInfo::ForPlaytime).has_value());
	EXPECT_FALSE(flags::ToolTipWords(TownDesireInfo::ForSleep).has_value());
}

TEST(TownDesireFlags, TheNeedShownIsTheDesireInHundredthsHeldToWhole)
{
	EXPECT_FLOAT_EQ(flags::NeedPercent(0.42f), 42.0f);
	EXPECT_FLOAT_EQ(flags::NeedPercent(1.7f), 100.0f);
	EXPECT_FLOAT_EQ(flags::NeedPercent(-0.2f), -20.0f);
}
