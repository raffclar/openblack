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

#include "Storms.h"

// The CHL's weather object (CREATE SCRIPT_OBJECT_TYPE_WEATHER_THING, script object type 15), listed newest first. It
// owns a storm built from the weather info of its subtype and keeps a copy of its descriptor; the CHANGE_*_PROPERTIES
// natives edit that copy and UpdateStats writes it back to the storm.

namespace openblack::ecs::components
{
struct WeatherThing
{
	weather::storms::StormDescriptor descriptor;
	weather::storms::StormId storm {weather::storms::k_NoStorm};
	bool affectedByWind {false};
};
} // namespace openblack::ecs::components

namespace openblack::weather::weather_thing
{
/// CHL CREATE: inner 100, outer 300, a life of 100 s, clouds at 500, the info's temperature, wetness, snowFall,
/// overCast and wind as the storm's bytes; the storm is made at once
entt::entity Create(const glm::vec3& position, uint32_t weatherInfo);
/// Every turn, after the land alignment's time update, each thing: a thing whose storm is gone forgets it (and would
/// be deleted if no script held it: openblack keeps it); with the wind on, the storm drifts by ComputeWeather's wind x
/// 0.01; the thing moves with its storm and copies its descriptor back
void ProcessWeatherThings();
/// The copy to the storm (or a new storm), and new lightning timers (min + rand(max - min))
void UpdateStats(entt::entity thing);
/// The things of the land, forgotten with the land (the registry is reset)
void Reset();

/// CHANGE_WEATHER_PROPERTIES: temperature (whole degrees), rain, snow and overcast x 100, fall speed
void SetWeatherProperties(entt::entity thing, float temperature, float rainfall, float snowfall, float overcast,
                          float fallSpeed);
/// CHANGE_TIME_FADE_PROPERTIES: lifeTime, fadeInTime
void SetTimeFadeProperties(entt::entity thing, float duration, float fadeTime);
/// CHANGE_CLOUD_PROPERTIES: blackness, number of clouds (int), elevation
void SetCloudProperties(entt::entity thing, float blackness, int32_t numClouds, float elevation);
/// CHANGE_LIGHTNING_PROPERTIES: sheet and fork ranges
void SetLightningProperties(entt::entity thing, float sheetMin, float sheetMax, float forkMin, float forkMax);
/// The wind bytes, clamp(v x 8, -127, 127)
void SetMovement(entt::entity thing, const glm::vec3& movement);
/// The storm's target and speed
void SetTarget(entt::entity thing, const glm::vec3& target);
void SetSpeed(entt::entity thing, float speed);
void SetAffectedByWind(entt::entity thing, bool on);
/// Only a weather thing says yes
[[nodiscard]] bool IsWeather(entt::entity thing);
} // namespace openblack::weather::weather_thing
