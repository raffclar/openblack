/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptWeatherRules.h"

#include <cmath>

#include <algorithm>

using namespace openblack::ecs;

namespace
{
constexpr float k_PercentOfWhole = 100.0f;
/// A storm the wind blows moves this much of the wind's bytes a turn
constexpr float k_WindDrift = 0.01f;

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

std::optional<float> script_weather::ClimateShare(const ClimateWind& climate, glm::vec2 point)
{
	// Within the square round its reach, then within its reach
	const auto& centre = climate.centre;
	const float outer = climate.outerRadius;
	if (!(centre.x - outer <= point.x && point.x <= centre.x + outer && centre.y - outer <= point.y &&
	      point.y <= centre.y + outer))
	{
		return std::nullopt;
	}
	const auto offset = point - centre;
	const float distanceSquared = (offset.x * offset.x) + (offset.y * offset.y);
	if (distanceSquared > outer * outer)
	{
		return std::nullopt;
	}
	const float inner = climate.innerRadius;
	if (!(distanceSquared > inner * inner))
	{
		return 1.0f;
	}
	const float share = 1.0f - ((std::sqrt(distanceSquared) - inner) / (outer - inner));
	if (!(share > 0.0f))
	{
		return 0.0f;
	}
	return std::min(share, 1.0f);
}

script_weather::Wind script_weather::DriftWind(Wind air, const ClimateWind& global, std::span<const ClimateWind> climates,
                                               glm::vec2 point)
{
	// The climates' part, each step cut to a whole number and kept to a byte
	Wind climatesPart {.x = WholeByte(global.windX), .z = WholeByte(global.windZ)};
	for (const auto& climate : climates)
	{
		const auto share = ClimateShare(climate, point);
		if (!share.has_value())
		{
			continue;
		}
		const auto add = [share](float wind, int8_t sum) {
			const double part = (static_cast<double>(WholeByte(wind)) * static_cast<double>(*share)) + sum;
			return static_cast<int8_t>(static_cast<uint8_t>(static_cast<int32_t>(part) & 0xff));
		};
		climatesPart.x = add(climate.windX, climatesPart.x);
		climatesPart.z = add(climate.windZ, climatesPart.z);
	}
	const auto sum = [](int8_t a, int8_t b) {
		return static_cast<int8_t>(std::clamp(static_cast<int32_t>(a) + static_cast<int32_t>(b), -128, 127));
	};
	return {.x = sum(air.x, climatesPart.x), .z = sum(air.z, climatesPart.z)};
}

glm::vec3 script_weather::Drift(glm::vec3 position, Wind wind)
{
	const auto along = [](int8_t byte, float from) {
		return static_cast<float>((static_cast<double>(byte) * static_cast<double>(k_WindDrift)) + static_cast<double>(from));
	};
	return {along(wind.x, position.x), position.y, along(wind.z, position.z)};
}
