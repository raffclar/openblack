/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

// The date part of the game info, what the climate reads: a game year lasts 36000 game turns (one hour at 10 turns
// per second), so a day is 98.56 turns (9.9 s). The land starts on 5 May 1998 at 18:05:30 (the map commands that
// change it are not used by the original lands). This is not the visual time of day (DayNightClock), which runs on
// its own.

namespace openblack::weather::calendar
{
/// Game turns per year (36000)
constexpr float k_TurnsPerYear = 36000.0f;
constexpr float k_SecondsInDay = 86400.0f; ///< Seconds in a day
constexpr float k_NumDaysInYear = 365.25f; ///< Days in a year

/// (turn + start) x (calendar seconds per turn) / 86400, without the whole years of the start date (the day of the
/// year is the same: they are multiples of 365.25 days)
[[nodiscard]] double GetDaysFromStart(uint32_t turn);
/// fmod(GetDaysFromStart, 365.25): what GetSeason, the month and the day of the month start from
[[nodiscard]] float GetDayOfYear(uint32_t turn);
/// The month 1..12 (the first whose cumulative day count is above the day of the year; 12 past the last)
[[nodiscard]] int32_t GetMonth(uint32_t turn);
/// The day of the month from 0 (the day of the year minus the days of the months before)
[[nodiscard]] float GetDayOfMonth(uint32_t turn);
/// 0 spring (from day 79), 1 summer (171), 2 autumn (263), 3 winter (from 354 and before 79); the climate info
/// columns are in this order
[[nodiscard]] uint32_t GetSeason(uint32_t turn);
} // namespace openblack::weather::calendar
