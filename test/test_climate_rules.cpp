/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <bit>

#include <gtest/gtest.h>

#include "ECS/ClimateRules.h"

using namespace openblack::ecs;

namespace
{
// The lowest and the highest desire to rain where the game gives the wind of half way
const float k_LowestHalfWay = std::bit_cast<float>(0x3EFFFB7Au);
const float k_HighestHalfWay = std::bit_cast<float>(0x3F000243u);
} // namespace

// The game multiplies the days by the double 0.01 and rounds one time. The float 0.01 gives one float step less.
TEST(ClimateRulesTest, RainedForRoundsTheDaysTimesTheDoubleOneTime)
{
	EXPECT_EQ(climate_rules::RainedFor(30), 0.3f);
	EXPECT_EQ(climate_rules::RainedFor(10), 0.1f);
	EXPECT_EQ(climate_rules::RainedFor(0), 0.0f);
	EXPECT_LT(30.0f * 0.01f, climate_rules::RainedFor(30));
}

TEST(ClimateRulesTest, WindIsTheSumOfTheLimitsAwayFromHalfWay)
{
	const auto wind = climate_rules::Wind(10.0f, 24.0f, 0.0f, 0.2f);
	EXPECT_EQ(wind.x, 34.0f);
	EXPECT_EQ(wind.y, 0.0f);
}

TEST(ClimateRulesTest, WindIsTwoTimesTheLowerLimitAtHalfWay)
{
	EXPECT_EQ(climate_rules::Wind(10.0f, 24.0f, 0.0f, 0.5f).x, 20.0f);
}

// Near half way the power of e rounds to 1 in the game's float steps, so these desires also give the wind of half way
TEST(ClimateRulesTest, WindOfHalfWayCoversTheDesiresWhosePowerRoundsToOne)
{
	EXPECT_EQ(climate_rules::Wind(10.0f, 24.0f, 0.0f, k_LowestHalfWay).x, 20.0f);
	EXPECT_EQ(climate_rules::Wind(10.0f, 24.0f, 0.0f, k_HighestHalfWay).x, 20.0f);
	EXPECT_EQ(climate_rules::Wind(10.0f, 24.0f, 0.0f, std::nextafter(k_LowestHalfWay, 0.0f)).x, 34.0f);
	EXPECT_EQ(climate_rules::Wind(10.0f, 24.0f, 0.0f, std::nextafter(k_HighestHalfWay, 1.0f)).x, 34.0f);
}

TEST(ClimateRulesTest, WindFollowsItsAngle)
{
	const auto wind = climate_rules::Wind(10.0f, 24.0f, 1.0f, 0.2f);
	EXPECT_NEAR(wind.x, 34.0f * std::cos(1.0f), 1e-4f);
	EXPECT_NEAR(wind.y, 34.0f * std::sin(1.0f), 1e-4f);
}

TEST(ClimateRulesTest, StormCloudUsesTheGameFloatSteps)
{
	EXPECT_EQ(climate_rules::StormCloud(30), 1.0f);
	// The double power of e gives 0x3F242466
	EXPECT_EQ(std::bit_cast<uint32_t>(climate_rules::StormCloud(20)), 0x3F242465u);
	EXPECT_EQ(climate_rules::StormCloud(20), climate_rules::StormCloud(40));
}

TEST(ClimateRulesTest, PercentCutsTowardsZero)
{
	EXPECT_EQ(climate_rules::Percent(1.0f), 100);
	EXPECT_EQ(climate_rules::Percent(climate_rules::StormCloud(20)), 64);
	EXPECT_EQ(climate_rules::Percent(climate_rules::StormCloud(-2)), 1);
	EXPECT_EQ(climate_rules::Percent(0.0f), 0);
}

TEST(ClimateRulesTest, StormSnowFallsAwayAboveFreezing)
{
	EXPECT_EQ(climate_rules::StormSnow(0), 100);
	EXPECT_EQ(climate_rules::StormSnow(1), 96);
	EXPECT_EQ(climate_rules::StormSnow(5), 36);
	EXPECT_EQ(climate_rules::StormSnow(10), 1);
	EXPECT_EQ(climate_rules::StormSnow(12), 0);
}

TEST(ClimateRulesTest, CellNearMiddleCutsTowardsZero)
{
	EXPECT_EQ(climate_rules::CellNearMiddle(0.0f, 5.5f, 100, 200), glm::ivec2(105, 200));
	EXPECT_EQ(climate_rules::CellNearMiddle(0.0f, 0.0f, 100, 200), glm::ivec2(100, 200));
}

// The game multiplies the hour factor by the month factor first, then the range of the season by that
TEST(ClimateRulesTest, TargetTemperatureMultipliesTheFactorsFirst)
{
	EXPECT_EQ(climate_rules::TargetTemperature(-10.0f, -7.0f, 0.9f, 0.7f), -8.11f);
	EXPECT_NE(climate_rules::TargetTemperature(-10.0f, -7.0f, 0.9f, 0.7f), ((-7.0f - -10.0f) * 0.9f * 0.7f) + -10.0f);
	EXPECT_EQ(climate_rules::TargetTemperature(5.0f, 15.0f, 1.0f, 1.0f), 15.0f);
}

// The game rounds the product with the double 1/30 to a float, then adds the double 0.5
TEST(ClimateRulesTest, StormSpeedRoundsTheProductFirst)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(climate_rules::StormSpeed(2.75f)), std::bit_cast<uint32_t>(0.5916666984558105f));
	EXPECT_EQ(climate_rules::StormSpeed(0.0f), 0.5f);
}

TEST(ClimateRulesTest, CalendarHasTheSecondsOfATurnAsAFloat)
{
	EXPECT_EQ(climate_rules::MakeCalendar().secondsPerTurn, 876.6f);
}

// The game adds the turn to the start in float precision, thus the days move on in steps of eight turns
TEST(ClimateRulesTest, DaysFromStartMoveOnInTheFloatStepsOfTheGame)
{
	const auto calendar = climate_rules::MakeCalendar();
	EXPECT_EQ(climate_rules::DaysFromStart(calendar, 0), 729895.3125f);
	EXPECT_EQ(climate_rules::DaysFromStart(calendar, 9), 729895.375f);
	EXPECT_EQ(climate_rules::DaysFromStart(calendar, 71), 729895.9375f);
	EXPECT_EQ(climate_rules::DaysFromStart(calendar, 36000), 730260.5f);
	EXPECT_EQ(climate_rules::DaysFromStart(calendar, 123456), 731147.875f);
}

TEST(ClimateRulesTest, DayOfMonthCountsFromTheEndOfTheLastMonth)
{
	EXPECT_EQ(climate_rules::DayOfMonth(5.5f), 5.5f);
	EXPECT_EQ(climate_rules::DayOfMonth(30.75f), 30.75f);
	EXPECT_EQ(climate_rules::DayOfMonth(31.0f), 0.0f);
	EXPECT_EQ(climate_rules::DayOfMonth(125.6875f), 5.6875f);
	EXPECT_EQ(climate_rules::DayOfMonth(363.5f), 30.5f);
	// After the end of the last month the day counts from that end
	EXPECT_EQ(climate_rules::DayOfMonth(364.5f), 0.5f);
	EXPECT_EQ(climate_rules::DayOfMonth(365.0f), 1.0f);
}
