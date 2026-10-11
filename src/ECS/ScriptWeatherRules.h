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

#include "ECS/Components/Weather.h"

/// How a script's weather commands change the storm a weather thing brought
namespace openblack::ecs::script_weather
{

/// A script's amount, 0 to 1, as the storm keeps it in whole percent, cut towards nought (and wrapping past a byte as
/// the storm's bytes do)
[[nodiscard]] int8_t Percent(float amount);

/// The weather the storm brings: the temperature it pulls towards, its rain, snow and cloud from 0 to 1, and how fast its
/// rain falls
void SetWeather(components::Storm& storm, float temperature, float rainfall, float snowfall, float overcast, float fallSpeed);
/// How many seconds the storm lasts and how many it takes to come in and to clear, counted from when it was made
void SetTimeFade(components::Storm& storm, float lastsFor, float fadeSeconds);
/// Its own clouds: how many (cut to a whole number), how dark from 0 to 1 and how high
void SetClouds(components::Storm& storm, float count, float shade, float height);
/// The least and most seconds between its flashes of sheet lightning with thunder, and between its forks of lightning
/// striking the ground; a most of 0 has none
void SetLightning(components::Storm& storm, float sheetMin, float sheetMax, float forkMin, float forkMax);
/// How far from its middle it is at full strength and how far it reaches
void SetInnerOuter(components::Storm& storm, float inner, float outer);

/// The waits to its next flash and next fork, drawn afresh whenever a script changes the storm: the fork first, then the
/// flash, each from its least up to its most
template <typename GameFloatRand>
void DrawLightningWaits(components::Storm& storm, GameFloatRand&& gameFloatRand)
{
	storm.boltTimer = gameFloatRand(storm.boltWait.y - storm.boltWait.x) + storm.boltWait.x;
	storm.thunderTimer = gameFloatRand(storm.thunderWait.y - storm.thunderWait.x) + storm.thunderWait.x;
}

} // namespace openblack::ecs::script_weather
