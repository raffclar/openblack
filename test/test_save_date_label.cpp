/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <ctime>

#include <chrono>
#include <format>
#include <string>

#include <gtest/gtest.h>

#include "ECS/SaveDateLabel.h"

using namespace openblack;

namespace
{

constexpr std::chrono::sys_seconds k_FixedTime {std::chrono::seconds {1'700'000'000}};

std::tm BrokenDownUtc(std::chrono::sys_seconds time)
{
	const std::time_t seconds = std::chrono::system_clock::to_time_t(time);
	std::tm result {};
#ifdef _WIN32
	gmtime_s(&result, &seconds);
#else
	gmtime_r(&seconds, &result);
#endif
	return result;
}

} // namespace

TEST(SaveDateLabel, FixedTimeGivesTimeThenLongDate)
{
	// 2023-11-14 22:13:20 UTC was a Tuesday
	EXPECT_EQ(ecs::save_date::FormatLabel(BrokenDownUtc(k_FixedTime)), "22:13:20 Tuesday, 14 November 2023");
}

TEST(SaveDateLabel, CLibraryPathGivesSameTextAsChronoPath)
{
	const std::string chrono = std::format("{:%H:%M:%S %A, %d %B %Y}", k_FixedTime);
	EXPECT_EQ(ecs::save_date::FormatLabel(BrokenDownUtc(k_FixedTime)), chrono);
}

TEST(SaveDateLabel, PaddedFieldsKeepTwoDigits)
{
	const std::chrono::sys_seconds early {std::chrono::seconds {1'704'067'205}}; // 2024-01-01 00:00:05 UTC
	EXPECT_EQ(ecs::save_date::FormatLabel(BrokenDownUtc(early)), "00:00:05 Monday, 01 January 2024");
	EXPECT_EQ(ecs::save_date::FormatLabel(BrokenDownUtc(early)), std::format("{:%H:%M:%S %A, %d %B %Y}", early));
}
