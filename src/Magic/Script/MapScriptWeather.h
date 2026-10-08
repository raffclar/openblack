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

#include <string>

#include <glm/vec3.hpp>

// The weather commands of the land scripts, called from
// FeatureScriptCommands.cpp. Positions are the script's "x,z" (openblack adds the land height as y).

namespace openblack::magic::map_script
{
/// case 60 CREATE_WEATHER_CLIMATE(id, info, pos, radius1, radius2): a climate at pos from GClimateInfo[info]
void CreateWeatherClimate(int32_t id, int32_t info, const glm::vec3& position, float radius1, float radius2);
/// CREATE_WEATHER_CLIMATE_RAIN(id, desire, dryDays, rainingDays, flags)
void CreateWeatherClimateRain(int32_t id, float desire, int32_t dryDays, int32_t rainingDays, int32_t flags);
/// CREATE_WEATHER_CLIMATE_TEMP(id, temperature, target)
void CreateWeatherClimateTemp(int32_t id, float temperature, float target);
/// CREATE_WEATHER_CLIMATE_WIND(id, windX, windZ, angle)
void CreateWeatherClimateWind(int32_t id, float windX, float windZ, float angle);
/// CREATE_WEATHER_STORM(climate, pos, age, numClouds, "inner,outer,fadeIn,life,strength,fallSpeed",
/// "blackness,elevation,forkMin,forkMax,sheetMin,sheetMax", "overcast,snow,temp,rain,windX,windZ", speed, target)
/// The third string is read with %d into the descriptor's bytes, 4 bytes each in that order, so each value
/// overwrites the three bytes after it: only temperature, rain and the wind survive (snow and overcast end 0 or -1).
/// No original land uses it.
void CreateWeatherStorm(int32_t climate, const glm::vec3& position, float age, int32_t numClouds, const std::string& shape,
                        const std::string& clouds, const std::string& weather, float speed, const glm::vec3& target);
} // namespace openblack::magic::map_script
