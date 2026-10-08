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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

// CHL natives of the weather, called from CHLApi.cpp. They pop the VM stack themselves (the handlers' order).

namespace openblack::magic::script
{
/// CREATE of a SCRIPT_OBJECT_TYPE_WEATHER_THING: a WeatherThing from GWeatherInfo[subtype]
entt::entity CreateWeatherThing(uint32_t subtype, const glm::vec3& position);
/// 123 CHANGE_WEATHER_PROPERTIES (storm, temperature, rainfall, snowfall, overcast, fallspeed)
void ChangeWeatherProperties();
/// 124 CHANGE_LIGHTNING_PROPERTIES (storm, sheetmin, sheetmax, forkmin, forkmax)
void ChangeLightningProperties();
/// 125 CHANGE_TIME_FADE_PROPERTIES (storm, duration, fadeTime)
void ChangeTimeFadeProperties();
/// 126 CHANGE_CLOUD_PROPERTIES (storm, numClouds, blackness, elevation): numClouds is truncated to an integer
void ChangeCloudProperties();
/// 401 PAUSE_UNPAUSE_CLIMATE_SYSTEM (the climate system runs when arg != 0)
void PauseUnpauseClimateSystem();
/// 402 PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM
void PauseUnpauseStormCreationInClimateSystem();
/// 404 KILL_STORMS_IN_AREA (position, radius)
void KillStormsInArea();
} // namespace openblack::magic::script
