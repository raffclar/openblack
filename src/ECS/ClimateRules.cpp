/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ClimateRules.h"

#include <algorithm>
#include <bit>
#include <iterator>

#include <glm/vec3.hpp>

#include "Common/GUtilsDistance.h"
#include "Common/X87.h"

namespace openblack::ecs::climate_rules
{

namespace
{
// The game keeps these constants as doubles
constexpr double k_RainPerDay = 0.01;
constexpr double k_HalfWayToRain = 0.5;
constexpr double k_WindSpread = 5.0;
constexpr double k_CloudiestTemperature = 30.0;
constexpr double k_CloudPerDegree = 1.0 / 15.0;
constexpr double k_SnowPerDegree = 0.2;
constexpr double k_Hundred = 100.0;
constexpr double k_StormSpeedPerWind = 1.0 / 30.0;
constexpr double k_LeastStormSpeed = 0.5;

// The calendar. The game keeps the parts of a day as floats.
constexpr float k_TurnsPerYear = 36000.0f;
constexpr float k_DaysInYear = 365.25f;
constexpr float k_SecondsInDay = 86400.0f;
constexpr float k_DayPerHour = std::bit_cast<float>(0x3D2AAAABu);
constexpr float k_DayPerMinute = std::bit_cast<float>(0x3A360B61u);
constexpr float k_DayPerSecond = std::bit_cast<float>(0x37422E45u);
constexpr int32_t k_StartYear = 1998;
constexpr int32_t k_StartMonth = 5;
constexpr int32_t k_StartDay = 5;
constexpr int32_t k_StartHour = 18;
constexpr int32_t k_StartMinute = 5;
constexpr int32_t k_StartSecond = 30;
} // namespace

float TargetTemperature(float minimum, float maximum, float hourFactor, float monthFactor)
{
	const float factor = hourFactor * monthFactor;
	return ((maximum - minimum) * factor) + minimum;
}

float StormSpeed(float mostWind)
{
	return x87::Add(x87::Mul(mostWind, k_StormSpeedPerWind), k_LeastStormSpeed);
}

Calendar MakeCalendar()
{
	// The game makes the start once as it starts, in double precision, and keeps the seconds of a turn as a float
	const float turnsPerDay = k_TurnsPerYear / k_DaysInYear;
	const float secondsPerTurn = k_SecondsInDay / turnsPerDay;
	const double turn = secondsPerTurn;
	const double day = k_SecondsInDay;
	const double secondsPerYear = static_cast<double>(k_SecondsInDay * k_DaysInYear);
	static_assert(k_StartMonth > 1);
	double start = static_cast<double>(k_StartYear) * secondsPerYear / turn;
	start = (static_cast<double>(k_MonthEnds[k_StartMonth - 2]) * day / turn) + start;
	start = (static_cast<double>(k_StartDay) * day / turn) + start;
	start += day * k_DayPerHour * k_StartHour / turn;
	start += day * k_DayPerMinute * k_StartMinute / turn;
	start += day * k_DayPerSecond * k_StartSecond / turn;
	return {.startTurn = start, .secondsPerTurn = secondsPerTurn};
}

float DaysFromStart(const Calendar& calendar, uint32_t turn)
{
	const float turns = x87::Add(static_cast<double>(turn), calendar.startTurn);
	return (turns * calendar.secondsPerTurn) / k_SecondsInDay;
}

float DayOfMonth(float dayOfYear)
{
	const auto next = std::ranges::find_if(k_MonthEnds, [dayOfYear](uint16_t end) { return dayOfYear < end; });
	if (next == k_MonthEnds.begin())
	{
		return dayOfYear;
	}
	return dayOfYear - static_cast<float>(*std::prev(next));
}

float RainedFor(int32_t rainingDays)
{
	return x87::Mul(static_cast<double>(rainingDays), k_RainPerDay);
}

glm::vec2 Wind(float minimum, float maximum, float angle, float rainDesire)
{
	// The weight is 1 only when e to the power of minus the square of the spread rounds to 1, else 0
	const float spread = x87::Mul(x87::Sub(rainDesire, k_HalfWayToRain), k_WindSpread);
	const auto weight = static_cast<float>(x87::ToInteger(x87::Exp(-(spread * spread))));

	const auto blend = [weight](float low, float high) { return high - ((high - low) * weight) + low; };
	const double cos = x87::Cos(angle);
	const double sin = x87::Sin(angle);
	return {
	    blend(x87::Mul(cos, minimum), x87::Mul(cos, maximum)),
	    blend(x87::Mul(sin, minimum), x87::Mul(sin, maximum)),
	};
}

float StormCloud(int8_t temperature)
{
	const float heat = x87::Mul(static_cast<double>(temperature) - k_CloudiestTemperature, k_CloudPerDegree);
	return x87::Exp(-(heat * heat));
}

int8_t Percent(float share)
{
	return static_cast<int8_t>(x87::ToInteger(x87::Mul(share, k_Hundred)));
}

int8_t StormSnow(int8_t temperature)
{
	const float cold = x87::Mul(static_cast<double>(temperature), k_SnowPerDegree);
	return Percent(x87::Exp(-(cold * cold)));
}

glm::ivec2 CellNearMiddle(float angle, float distance, uint16_t middleX, uint16_t middleZ)
{
	// The product with the wide sine or cosine rounds one time, then the sum with the middle cell rounds
	const float x = x87::Mul(x87::Cos(angle), distance) + static_cast<float>(middleX);
	const float z = x87::Mul(x87::Sin(angle), distance) + static_cast<float>(middleZ);
	return {x87::ToInteger(x), x87::ToInteger(z)};
}

uint32_t LocalStormRadius(glm::ivec2 cell, const map_coords::MapCoords& centre)
{
	// The cell and the middle become metres with one float product each; the heights are not part of the distance
	const glm::vec3 point {static_cast<float>(cell.x) * map_coords::k_CellSize, 0.0f,
	                       static_cast<float>(cell.y) * map_coords::k_CellSize};
	const glm::vec3 middle {map_coords::ToMetres(centre.x), 0.0f, map_coords::ToMetres(centre.z)};
	return static_cast<uint32_t>(x87::ToInteger(gutils::GetDistance(point, middle)));
}

} // namespace openblack::ecs::climate_rules
