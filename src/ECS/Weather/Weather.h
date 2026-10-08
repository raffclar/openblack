/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

#include "WeatherInfo.h"

// The weather queries the rest of the game uses (the climate's getters). Every query samples ComputeWeather at a
// point: the atmosphere's 40 m grid (the storms registered in it) plus the climates' temperature and wind. The point
// is x, z in metres and y the absolute height (the temperature drops above 50 m). Wiki: docs/bw1-notes/magic.md
// ("Time and weather").
//   Weather.h          these queries (the small header other systems include)
//   WeatherInfo.h      the 8-byte WeatherInfo and its byte arithmetic
//   Atmos.{h,cpp}      the atmosphere: the 128 x 128 grid of 40 m cells
//   Storms.{h,cpp}     the registered weather volumes (miracle storms, climate storms)
//   Climate.{h,cpp}    the world and local climates, ComputeWeather, ProcessAll, natural storms
//   Calendar.{h,cpp}   the game's date (day of the year, month, season)
//   WeatherThing.{h,cpp}  the CHL weather objects (CREATE WEATHER_THING, CHANGE_*_PROPERTIES)

namespace openblack::weather
{
/// The weather at a point. `smooth` samples the grid bilinearly (the camera's choice); every getter below passes
/// false.
[[nodiscard]] WeatherInfo ComputeWeather(const glm::vec3& point, bool smooth = false);

/// Rain byte > 0 (false while there is no world climate)
[[nodiscard]] bool IsRainingAt(const glm::vec3& point);
/// Snow byte > 0
[[nodiscard]] bool IsSnowingAt(const glm::vec3& point);
/// Snow-cover byte > 0
[[nodiscard]] bool IsSnowCoveredAt(const glm::vec3& point);
/// The rain byte (0..127; 0 without a world climate)
[[nodiscard]] float GetRainAt(const glm::vec3& point);
/// The snow lying on the ground (snowCover; IsSnowingAt reads the falling snow)
[[nodiscard]] float GetSnowAt(const glm::vec3& point);
/// max(rain, snow lying on the ground). The fire cools with 1 + 0.01 x this,
/// fireballs steam and trees grow with it.
[[nodiscard]] float GetMaxRainingOrSnowingAt(const glm::vec3& point);
/// The temperature byte (degrees; about 20 on the Land 1 lowlands).
/// NOT the fire's ambient temperature: that is a constant 24.7 (fire::AmbientTemperature).
[[nodiscard]] float GetTemperatureAt(const glm::vec3& point);
/// The wind bytes
[[nodiscard]] float GetWindXAt(const glm::vec3& point);
[[nodiscard]] float GetWindZAt(const glm::vec3& point);
/// The wind as a vector, (windX / 8, 0, windZ / 8). The fire (unused result), the cloud particles and the fireballs'
/// gravity read it.
[[nodiscard]] glm::vec3 GetWindAt(const glm::vec3& point, bool smooth = false);
} // namespace openblack::weather
