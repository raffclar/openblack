/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The weather getters (Weather.h).

#include <algorithm>

#include "Climate.h"
#include "Weather.h"

using namespace openblack;
using namespace openblack::weather;

WeatherInfo weather::ComputeWeather(const glm::vec3& point, bool smooth)
{
	return climate::ComputeWeather(point, smooth);
}

// IsRainingAt, IsSnowingAt, IsSnowCoveredAt, GetRainAt, GetSnowAt: 0 / false while there is no world climate; the
// others make it

bool weather::IsRainingAt(const glm::vec3& point)
{
	return climate::HasWorld() && climate::ComputeWeather(point, false).rain > 0;
}

bool weather::IsSnowingAt(const glm::vec3& point)
{
	return climate::HasWorld() && climate::ComputeWeather(point, false).snow > 0;
}

bool weather::IsSnowCoveredAt(const glm::vec3& point)
{
	return climate::HasWorld() && climate::ComputeWeather(point, false).snowCover > 0;
}

float weather::GetRainAt(const glm::vec3& point)
{
	return climate::HasWorld() ? static_cast<float>(climate::ComputeWeather(point, false).rain) : 0.0f;
}

float weather::GetSnowAt(const glm::vec3& point)
{
	// the snow lying on the ground, not the falling snow
	return climate::HasWorld() ? static_cast<float>(climate::ComputeWeather(point, false).snowCover) : 0.0f;
}

float weather::GetMaxRainingOrSnowingAt(const glm::vec3& point)
{
	// the larger of the snow lying on the ground and the rain (signed bytes)
	const auto weather = climate::ComputeWeather(point, false);
	return static_cast<float>(std::max(weather.snowCover, weather.rain));
}

float weather::GetTemperatureAt(const glm::vec3& point)
{
	return static_cast<float>(climate::ComputeWeather(point, false).temperature);
}

float weather::GetWindXAt(const glm::vec3& point)
{
	return static_cast<float>(climate::ComputeWeather(point, false).windX);
}

float weather::GetWindZAt(const glm::vec3& point)
{
	return static_cast<float>(climate::ComputeWeather(point, false).windZ);
}

glm::vec3 weather::GetWindAt(const glm::vec3& point, bool smooth)
{
	const auto weather = climate::ComputeWeather(point, smooth);
	return {static_cast<float>(weather.windX) * 0.125f, 0.0f, static_cast<float>(weather.windZ) * 0.125f};
}
