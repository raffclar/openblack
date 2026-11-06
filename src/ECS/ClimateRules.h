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

#include <array>

#include <glm/vec2.hpp>

/// The formulas of the climates and of the storms that they make, with the float steps of the game
namespace openblack::ecs::climate_rules
{

/// The raining days of a climate as a share of rain, which the climate compares with the limits of its season
[[nodiscard]] float RainedFor(int32_t rainingDays);

/// The wind of a climate (x, z) along its angle in radians. Usually the wind is the sum of the two limits of the season.
/// When the desire to rain is at half way or very near it, the wind is two times the lower limit.
[[nodiscard]] glm::vec2 Wind(float minimum, float maximum, float angle, float rainDesire);

/// How cloudy a new storm is, from 0 to 1, by its temperature: 1 at 30 degrees and less above and below it
[[nodiscard]] float StormCloud(int8_t temperature);

/// A share from 0 to 1 in whole percent, cut towards zero
[[nodiscard]] int8_t Percent(float share);

/// The snow percentage of a new storm with a temperature of 0 or more: all snow at 0, less snow as it gets warmer
[[nodiscard]] int8_t StormSnow(int8_t temperature);

/// The cell at a distance in cells and an angle in radians from the middle cell of a climate, cut towards zero
[[nodiscard]] glm::ivec2 CellNearMiddle(float angle, float distance, uint16_t middleX, uint16_t middleZ);

/// The target temperature of a climate: the range of its season times the hour factor and the month factor. The game
/// multiplies the two factors first.
[[nodiscard]] float TargetTemperature(float minimum, float maximum, float hourFactor, float monthFactor);

/// The speed of the storms of a climate from the most wind of its season
[[nodiscard]] float StormSpeed(float mostWind);

/// The day of the year after each month ends, as the game counts its months: September, October and November have 30
/// days each, so the year has 364 days in its months
inline constexpr std::array<uint16_t, 12> k_MonthEnds = {31, 59, 90, 120, 151, 181, 212, 243, 273, 303, 333, 364};

/// The calendar of the game: the turn of 5 May 1998 at 18:05:30, counted from the year 0, and the seconds of one turn
struct Calendar
{
	double startTurn;
	float secondsPerTurn;
};

/// The calendar of the game, with the constants of the game
[[nodiscard]] Calendar MakeCalendar();

/// The days from the year 0 to a turn. The game adds the turn to the start in float precision, thus the days move on
/// in steps of some turns.
[[nodiscard]] float DaysFromStart(const Calendar& calendar, uint32_t turn);

/// The day of the month from the day of the year. A day of the year after the end of the last month counts from that
/// end, thus the last day and a quarter of the year are the days 0 and 1.
[[nodiscard]] float DayOfMonth(float dayOfYear);

} // namespace openblack::ecs::climate_rules
