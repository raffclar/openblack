/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <ctime>

/// What the scripts read of the date and the time of the player's computer, from its local time, as the game gives
/// them. The game gives these values in a game for more than one player too.
namespace openblack::script::real_time
{

/// An hour as a part of a second, as the game keeps it in a float
inline constexpr float k_HoursPerSecond = 0.00027777778450399637f;

/// The time of day in hours: the hour, and the minutes and seconds as a part of an hour
[[nodiscard]] inline float TimeOfDay(const std::tm& time)
{
	return static_cast<float>((time.tm_min * 60) + time.tm_sec) * k_HoursPerSecond + static_cast<float>(time.tm_hour);
}

/// The day of the month, 1 to 31
[[nodiscard]] inline float DayOfMonth(const std::tm& time)
{
	return static_cast<float>(time.tm_mday);
}

/// The day of the week, 1 (Sunday) to 7 (Saturday)
[[nodiscard]] inline float DayOfWeek(const std::tm& time)
{
	return static_cast<float>(time.tm_wday) + 1.0f;
}

/// The month, 1 (January) to 12 (December)
[[nodiscard]] inline float Month(const std::tm& time)
{
	return static_cast<float>(time.tm_mon) + 1.0f;
}

/// The year, for example 2001
[[nodiscard]] inline float Year(const std::tm& time)
{
	return static_cast<float>(time.tm_year) + 1900.0f;
}

} // namespace openblack::script::real_time
