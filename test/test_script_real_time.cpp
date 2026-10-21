/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <ctime>

#include <gtest/gtest.h>

#include "ScriptHeaders/ScriptRealTime.h"

namespace real_time = openblack::script::real_time;

namespace
{
/// Saturday the 21st of October 2026, 18:45:30
std::tm Evening()
{
	std::tm time {};
	time.tm_year = 126;
	time.tm_mon = 9;
	time.tm_mday = 21;
	time.tm_wday = 6;
	time.tm_hour = 18;
	time.tm_min = 45;
	time.tm_sec = 30;
	return time;
}
} // namespace

TEST(ScriptRealTime, TheTimeIsInHoursWithTheMinutesAndSecondsAsAPart)
{
	// 2730 seconds times the game's float of a 3600th, rounded to a float, then the hour added and rounded
	const float part = 2730.0f * 0.00027777778450399637f;
	EXPECT_EQ(real_time::TimeOfDay(Evening()), part + 18.0f);
	EXPECT_NEAR(real_time::TimeOfDay(Evening()), 18.758333f, 1e-5f);
	std::tm midnight {};
	EXPECT_EQ(real_time::TimeOfDay(midnight), 0.0f);
}

TEST(ScriptRealTime, TheDateCountsFromOne)
{
	EXPECT_EQ(real_time::DayOfMonth(Evening()), 21.0f);
	EXPECT_EQ(real_time::DayOfWeek(Evening()), 7.0f);
	EXPECT_EQ(real_time::Month(Evening()), 10.0f);
	EXPECT_EQ(real_time::Year(Evening()), 2026.0f);
}
