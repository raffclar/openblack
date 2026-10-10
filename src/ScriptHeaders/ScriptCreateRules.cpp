/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptCreateRules.h"

#include <cmath>

#include "InfoConstants.h"
#include "ScriptHeaders/ScriptTimers.h"

namespace openblack::script::create_rules
{

namespace
{

/// Degrees to radians, as the game's single-precision constant
constexpr float k_RadiansPerDegree = 0.0174532924f;
/// One turn, and its inverse, in radians
constexpr float k_Turn = 6.28318548f;
constexpr float k_TurnsPerRadian = 0.159154937f;

/// The number of miracles, none included
constexpr int32_t k_MagicTypeCount = 42;

// A scripted weather thing's storm: its size, how long it takes to come in and to clear, how long it lasts, how strong
// it is, how high its clouds are and how fast its rain falls
constexpr float k_WeatherInnerRadius = 100.0f;
constexpr float k_WeatherOuterRadius = 300.0f;
constexpr float k_WeatherFadeSeconds = 10.0f;
constexpr float k_WeatherLastsFor = 100.0f;
constexpr float k_WeatherStrength = 1.0f;
constexpr float k_WeatherCloudHeight = 500.0f;
constexpr float k_WeatherRainSpeed = 1.0f;

} // namespace

bool IsCreatableType(int32_t type)
{
	return type >= k_FirstCreateType && type <= k_LastCreateType;
}

float AngleFromDegrees(float degrees)
{
	return degrees * k_RadiansPerDegree;
}

float TreeAngle(float radians)
{
	const auto turns = static_cast<int32_t>(radians * k_TurnsPerRadian);
	return radians - static_cast<float>(turns) * k_Turn;
}

bool IsRow(uint32_t subtype, std::size_t rows)
{
	return static_cast<std::size_t>(subtype) < rows;
}

std::optional<MagicType> MagicTypeFromScript(int32_t number)
{
	if (number < 0 || number >= k_MagicTypeCount)
	{
		return std::nullopt;
	}
	return static_cast<MagicType>(number);
}

uint32_t DispenserTurns(float seconds, float buildingPeriodTurns)
{
	if (seconds > 0.0f)
	{
		const auto turns = timers::TurnsFor(seconds);
		return turns > 0 ? static_cast<uint32_t>(turns) : 0;
	}
	return static_cast<uint32_t>(buildingPeriodTurns);
}

ecs::systems::ScriptStorm WeatherThingStorm(const GWeatherInfo& info, const glm::vec3& centre)
{
	// Only the kind's weather is taken from its row; its own fading time, life and strength are not used
	return {
	    .centre = centre,
	    .innerRadius = k_WeatherInnerRadius,
	    .outerRadius = k_WeatherOuterRadius,
	    .fadeSeconds = k_WeatherFadeSeconds,
	    .lastsFor = k_WeatherLastsFor,
	    .strength = k_WeatherStrength,
	    .cloudHeight = k_WeatherCloudHeight,
	    .rainSpeed = k_WeatherRainSpeed,
	    .effect =
	        {
	            .temperature = static_cast<int8_t>(info.temperature),
	            .rain = static_cast<int8_t>(info.wetness),
	            .snow = static_cast<int8_t>(info.snowFall),
	            .overcast = static_cast<int8_t>(info.overCast),
	            .windX = static_cast<int8_t>(info.wind.x),
	            .windZ = static_cast<int8_t>(info.wind.y),
	        },
	};
}

} // namespace openblack::script::create_rules
