/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Calendar.h"

#include <cmath>

#include <array>

using namespace openblack::weather;

namespace
{
/// The month lengths, made cumulative by the game info's constructor (October has 30 days in the original: 364 in
/// all)
constexpr std::array<uint16_t, 12> k_MonthEnds = {31, 59, 90, 120, 151, 181, 212, 243, 273, 303, 333, 364};
/// GetSeason's table: the first day of spring, summer, autumn and winter (winter also runs up to day 79)
constexpr std::array<uint16_t, 4> k_SeasonStarts = {79, 171, 263, 354};

/// Turns per year / 365.25 (turns per day), and 86400 / that (calendar seconds per turn), both floats
constexpr float k_TurnsPerDay = calendar::k_TurnsPerYear / calendar::k_NumDaysInYear;
constexpr float k_SecondsPerTurn = calendar::k_SecondsInDay / k_TurnsPerDay;

/// The start date 5 May 1998 and time 18:05:30: the months before May (120 days, from the cumulative table), the day
/// of the month (5, added whole), the hours, minutes and seconds; in turns
[[nodiscard]] constexpr double StartTurns()
{
	constexpr int k_Month = 5;
	constexpr int k_Day = 5;
	constexpr int k_Hour = 18;
	constexpr int k_Minute = 5;
	constexpr int k_Second = 30;
	double seconds = static_cast<double>(k_MonthEnds[k_Month - 2]) * calendar::k_SecondsInDay;
	seconds += static_cast<double>(k_Day) * calendar::k_SecondsInDay;
	seconds += static_cast<double>(k_Hour) * 3600.0 + static_cast<double>(k_Minute) * 60.0 + static_cast<double>(k_Second);
	return seconds / static_cast<double>(k_SecondsPerTurn);
}
} // namespace

double calendar::GetDaysFromStart(uint32_t turn)
{
	constexpr double k_Start = StartTurns();
	return (static_cast<double>(turn) + k_Start) * static_cast<double>(k_SecondsPerTurn) / static_cast<double>(k_SecondsInDay);
}

float calendar::GetDayOfYear(uint32_t turn)
{
	return static_cast<float>(std::fmod(GetDaysFromStart(turn), static_cast<double>(k_NumDaysInYear)));
}

int32_t calendar::GetMonth(uint32_t turn)
{
	const float day = GetDayOfYear(turn);
	for (size_t i = 0; i < k_MonthEnds.size(); ++i)
	{
		if (static_cast<float>(k_MonthEnds[i]) > day)
		{
			return static_cast<int32_t>(i) + 1;
		}
	}
	return 12;
}

float calendar::GetDayOfMonth(uint32_t turn)
{
	const float day = GetDayOfYear(turn);
	size_t i = 0;
	while (i < k_MonthEnds.size() && day >= static_cast<float>(k_MonthEnds[i]))
	{
		++i;
	}
	return i == 0 ? day : day - static_cast<float>(k_MonthEnds[i - 1]);
}

uint32_t calendar::GetSeason(uint32_t turn)
{
	const float day = GetDayOfYear(turn);
	uint32_t i = 0;
	while (i < k_SeasonStarts.size() && !(static_cast<float>(k_SeasonStarts[i]) > day))
	{
		++i;
	}
	return i == 0 ? 3 : i - 1;
}
