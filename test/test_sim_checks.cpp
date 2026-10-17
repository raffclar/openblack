/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <initializer_list>
#include <limits>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Debug/SimChecks.h"

using namespace openblack::sim_checks;

TEST(SimChecks, NumbersThatAreNotNumbersFlag)
{
	const auto nan = CheckValue("x", std::numeric_limits<double>::quiet_NaN(), std::nullopt, 3);
	ASSERT_TRUE(nan.has_value());
	EXPECT_EQ(nan->kind, FlagKind::NotANumber);
	EXPECT_EQ(nan->turn, 3u);
	EXPECT_TRUE(CheckValue("x", std::numeric_limits<double>::infinity(), std::nullopt, 0).has_value());
	EXPECT_FALSE(CheckValue("x", 12345.0, std::nullopt, 0).has_value());
}

TEST(SimChecks, ValuesOutsideTheirRangeFlag)
{
	const Range unit {.lowest = 0.0f, .highest = 1.0f};
	EXPECT_FALSE(CheckValue("x", 0.0, unit, 0).has_value());
	EXPECT_FALSE(CheckValue("x", 1.0, unit, 0).has_value());
	const auto above = CheckValue("x", 1.5, unit, 0);
	ASSERT_TRUE(above.has_value());
	EXPECT_EQ(above->kind, FlagKind::OutOfRange);
	EXPECT_TRUE(CheckValue("x", -0.1, unit, 0).has_value());
}

TEST(SimChecks, AnOwnLimitComesBeforeTheLimitOfTheSlot)
{
	const std::vector<StuckLimit> limits {{.key = "plan:EAT", .turns = 50}, {.key = "plan:*", .turns = 500}};
	EXPECT_EQ(LimitFor(limits, "plan", "EAT"), 50u);
	EXPECT_EQ(LimitFor(limits, "plan", "SLEEP"), 500u);
	EXPECT_EQ(LimitFor(limits, "activity", "SLEEP"), std::nullopt);
}

TEST(SimChecks, APlanPastItsLimitFlagsOnce)
{
	StuckTracker stuck({{.key = "plan:EAT", .turns = 10}});
	EXPECT_FALSE(stuck.Observe("plan", "EAT", 1, 0).has_value());
	EXPECT_FALSE(stuck.Observe("plan", "EAT", 1, 10).has_value());
	const auto flag = stuck.Observe("plan", "EAT", 1, 11);
	ASSERT_TRUE(flag.has_value());
	EXPECT_EQ(flag->kind, FlagKind::Stuck);
	EXPECT_EQ(flag->what, "plan:EAT");
	EXPECT_FALSE(stuck.Observe("plan", "EAT", 1, 30).has_value());
}

TEST(SimChecks, ANewPlanStartsTheCountAgain)
{
	StuckTracker stuck({{.key = "plan:*", .turns = 10}});
	EXPECT_FALSE(stuck.Observe("plan", "EAT", 1, 0).has_value());
	// The same action, but a new plan: its serial changed
	EXPECT_FALSE(stuck.Observe("plan", "EAT", 2, 8).has_value());
	EXPECT_FALSE(stuck.Observe("plan", "EAT", 2, 16).has_value());
	EXPECT_TRUE(stuck.Observe("plan", "EAT", 2, 19).has_value());
}

TEST(SimChecks, TurnsThatStartAgainDoNotFlag)
{
	StuckTracker stuck({{.key = "plan:*", .turns = 10}});
	EXPECT_FALSE(stuck.Observe("plan", "EAT", 1, 500).has_value());
	EXPECT_FALSE(stuck.Observe("plan", "EAT", 1, 5).has_value());
	EXPECT_FALSE(stuck.Observe("plan", "EAT", 1, 15).has_value());
}

TEST(SimChecks, WithNoLimitTheTrackerKeepsTheLongestTime)
{
	StuckTracker stuck;
	EXPECT_FALSE(stuck.Observe("plan", "EAT", 1, 0).has_value());
	EXPECT_FALSE(stuck.Observe("plan", "EAT", 1, 400).has_value());
	EXPECT_FALSE(stuck.Observe("plan", "SLEEP", 2, 410).has_value());
	EXPECT_FALSE(stuck.Observe("plan", "SLEEP", 2, 420).has_value());
	const auto longest = stuck.LongestRuns();
	ASSERT_EQ(longest.size(), 2u);
	EXPECT_EQ(longest[0].key, "plan:EAT");
	EXPECT_EQ(longest[0].turns, 400u);
	EXPECT_EQ(longest[0].limit, std::nullopt);
	EXPECT_EQ(longest[1].turns, 10u);
}

TEST(SimChecks, AListThatAlwaysGrowsFlags)
{
	GrowthTracker growth({.window = 5, .leastSize = 8, .factor = 2.0f});
	std::optional<Flag> flag;
	for (uint32_t i = 0; i < 5 && !flag.has_value(); ++i)
	{
		flag = growth.Observe("list", 4 + i * 2, i);
	}
	ASSERT_TRUE(flag.has_value());
	EXPECT_EQ(flag->kind, FlagKind::Growth);
	EXPECT_EQ(flag->turn, 4u);
	// It flags once while it continues to grow
	EXPECT_FALSE(growth.Observe("list", 20, 5).has_value());
}

TEST(SimChecks, AListThatGetsSmallerDoesNotFlag)
{
	GrowthTracker growth({.window = 4, .leastSize = 8, .factor = 2.0f});
	for (const auto size : {10u, 20u, 15u, 40u})
	{
		EXPECT_FALSE(growth.Observe("list", size, 0).has_value());
	}
}

TEST(SimChecks, ASmallOrSteadyListDoesNotFlag)
{
	GrowthTracker growth({.window = 3, .leastSize = 100, .factor = 2.0f});
	for (const auto size : {10u, 20u, 40u, 80u})
	{
		EXPECT_FALSE(growth.Observe("small", size, 0).has_value());
	}
	for (int i = 0; i < 10; ++i)
	{
		EXPECT_FALSE(growth.Observe("steady", 500, 0).has_value());
	}
}

TEST(SimChecks, ATurnTimeThatGoesUpFlags)
{
	TurnTimeTrend trend({.warmUp = 2, .window = 3, .factor = 1.5f, .leastRiseMs = 0.5f});
	// The warm up, then the base
	for (const auto ms : {9.0f, 9.0f, 2.0f, 2.0f, 2.0f})
	{
		EXPECT_FALSE(trend.Observe(ms, 0).has_value());
	}
	ASSERT_TRUE(trend.BaseMs().has_value());
	EXPECT_FLOAT_EQ(*trend.BaseMs(), 2.0f);
	EXPECT_FALSE(trend.Observe(2.1f, 1).has_value());
	EXPECT_FALSE(trend.Observe(4.0f, 2).has_value());
	const auto flag = trend.Observe(5.0f, 3);
	ASSERT_TRUE(flag.has_value());
	EXPECT_EQ(flag->kind, FlagKind::TurnTime);
	EXPECT_FALSE(trend.Observe(5.0f, 4).has_value());
}

TEST(SimChecks, ASmallRiseOfTheTurnTimeDoesNotFlag)
{
	TurnTimeTrend trend({.warmUp = 0, .window = 2, .factor = 1.5f, .leastRiseMs = 0.5f});
	for (const auto ms : {0.1f, 0.1f, 0.3f, 0.3f, 0.3f})
	{
		EXPECT_FALSE(trend.Observe(ms, 0).has_value());
	}
}

TEST(SimChecks, ErrorsAndAssertsInTheLogFlag)
{
	EXPECT_TRUE(CheckLogMessage(LogLevel::Error, "the file is missing", 0).has_value());
	EXPECT_TRUE(CheckLogMessage(LogLevel::Critical, "stop", 0).has_value());
	EXPECT_TRUE(CheckLogMessage(LogLevel::Warning, "Assertion failed: x", 0).has_value());
	EXPECT_FALSE(CheckLogMessage(LogLevel::Warning, "the creature is hungry", 0).has_value());
	EXPECT_FALSE(CheckLogMessage(LogLevel::Other, "ass", 0).has_value());
}
