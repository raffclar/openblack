/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <ctime>

#include <optional>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "Common/MachineClock.h"
#include "ECS/SaveGameFiles.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Help/GuidanceRules.h"
#include "ScriptHeaders/ScriptRealTime.h"

using namespace openblack;

namespace
{
/// The seconds since 1970 of a local date and time, by the computer's own time zone
int64_t LocalSeconds(int year, int month, int day, int hour, int minute)
{
	std::tm local {};
	local.tm_year = year - 1900;
	local.tm_mon = month - 1;
	local.tm_mday = day;
	local.tm_hour = hour;
	local.tm_min = minute;
	local.tm_isdst = -1;
	return static_cast<int64_t>(std::mktime(&local));
}
} // namespace

// The local time of a count of seconds gives back the local date and time that made it
TEST(RealClock, TheLocalTimeOfADateIsThatDate)
{
	const auto local = machine_clock::LocalTimeOf(LocalSeconds(2026, 4, 1, 23, 30));
	EXPECT_EQ(local.tm_year, 126);
	EXPECT_EQ(local.tm_mon, 3);
	EXPECT_EQ(local.tm_mday, 1);
	EXPECT_EQ(local.tm_hour, 23);
	EXPECT_EQ(local.tm_min, 30);
	// 1 April 2026 is a Wednesday, the fourth day of the week for the scripts
	EXPECT_EQ(script::real_time::DayOfWeek(local), 4.0f);
	EXPECT_EQ(script::real_time::Month(local), 4.0f);
	EXPECT_EQ(script::real_time::DayOfMonth(local), 1.0f);
}

// A date read from a set clock goes through the same rules as one from the computer's clock
TEST(RealClock, ASetClockGivesTheNightOfItsDate)
{
	const auto lateEvening = machine_clock::LocalTimeOf(LocalSeconds(2026, 4, 1, 20, 50));
	EXPECT_TRUE(guidance::ClockSaysNight(lateEvening.tm_hour, lateEvening.tm_min));
	const auto evening = machine_clock::LocalTimeOf(LocalSeconds(2026, 4, 1, 21, 30));
	EXPECT_FALSE(guidance::ClockSaysNight(evening.tm_hour, evening.tm_min));
}

// The clock reads the date set now, whatever the ticks since its start
TEST(RealClock, APinForADateNowTakesOffTheWholeSecondsOfTicks)
{
	EXPECT_EQ(ecs::systems::PinForDateNow(1000, 0), 1000);
	EXPECT_EQ(ecs::systems::PinForDateNow(1000, 999), 1000);
	EXPECT_EQ(ecs::systems::PinForDateNow(1000, 2500), 998);
}

// The scripts are told the game's own words when they read the clock in a game for more than one player
TEST(RealClock, TheScriptsAreToldTheClockIsNotForMultiplayer)
{
	EXPECT_EQ(script::real_time::k_NotMultiplayerFriendly, "This is not multiplayer friendly yet!");
}

// A saved game's label is the time, a space and the date
TEST(RealClock, ASaveLabelIsTheTimeThenTheDate)
{
	EXPECT_EQ(ecs::save_files::SaveDateLabel(u"23:30:00", u"Wednesday, 1 April 2026"), u"23:30:00 Wednesday, 1 April 2026");
}

// The two parts count their characters and one more each: at 62 or more together the label is empty
TEST(RealClock, ASaveLabelTooLongIsEmpty)
{
	const std::u16string time(8, u'1');
	// 9 and 52 make 61: the label is kept
	const std::u16string date51(51, u'2');
	EXPECT_EQ(ecs::save_files::SaveDateLabel(time, date51).size(), 60u);
	// 9 and 53 make 62: the label is empty
	const std::u16string date52(52, u'2');
	EXPECT_TRUE(ecs::save_files::SaveDateLabel(time, date52).empty());
}

// A part that the computer could not give is empty and counts as nothing; the space stays
TEST(RealClock, ASaveLabelPartNotGivenIsEmpty)
{
	EXPECT_EQ(ecs::save_files::SaveDateLabel(u"23:30:00", std::nullopt), u"23:30:00 ");
	EXPECT_EQ(ecs::save_files::SaveDateLabel(std::nullopt, u"1 April"), u" 1 April");
	const std::u16string date60(60, u'2');
	EXPECT_EQ(ecs::save_files::SaveDateLabel(std::nullopt, date60).size(), 61u);
}
