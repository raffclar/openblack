/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptWeatherRules.h"

using namespace openblack::ecs;

namespace
{
constexpr float k_PercentOfWhole = 100.0f;

int8_t WholeByte(float value)
{
	// Cut towards nought, keeping the low byte
	return static_cast<int8_t>(static_cast<uint8_t>(static_cast<int32_t>(value) & 0xff));
}
} // namespace

int8_t script_weather::Percent(float amount)
{
	return WholeByte(amount * k_PercentOfWhole);
}

void script_weather::SetWeather(components::Storm& storm, float temperature, float rainfall, float snowfall, float overcast,
                                float fallSpeed)
{
	storm.effect.temperature = WholeByte(temperature);
	storm.effect.rain = Percent(rainfall);
	storm.effect.snow = Percent(snowfall);
	storm.effect.overcast = Percent(overcast);
	storm.rainSpeed = fallSpeed;
}

void script_weather::SetTimeFade(components::Storm& storm, float lastsFor, float fadeSeconds)
{
	storm.lastsFor = lastsFor;
	storm.fadeTime = fadeSeconds;
}

void script_weather::SetClouds(components::Storm& storm, float count, float shade, float height)
{
	storm.cloudCount = static_cast<int32_t>(count);
	storm.cloudShade = shade;
	storm.cloudHeight = height;
}

void script_weather::SetLightning(components::Storm& storm, float sheetMin, float sheetMax, float forkMin, float forkMax)
{
	storm.thunderWait = {sheetMin, sheetMax};
	storm.boltWait = {forkMin, forkMax};
}

void script_weather::SetInnerOuter(components::Storm& storm, float inner, float outer)
{
	storm.innerRadius = inner;
	storm.outerRadius = outer;
}
