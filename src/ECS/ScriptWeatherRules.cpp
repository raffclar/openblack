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
#include <cstdlib>

#include <algorithm>
#include <array>
#include <string>
#include <type_traits>

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
/// Reads up to as many comma-separated numbers as the list holds, the way the C library's scanning does: each may follow
/// spaces, and reading stops at the first that is missing or not followed by a comma (the last needs none). Returns how
/// many were read; the rest keep their values.
template <typename T, size_t N>
size_t ScanList(std::string_view text, std::array<T, N>& values)
{
	const std::string copy(text);
	const char* next = copy.c_str();
	for (size_t i = 0; i < N; ++i)
	{
		if (i > 0)
		{
			if (*next != ',')
			{
				return i;
			}
			++next;
		}
		char* end = nullptr;
		T value {};
		if constexpr (std::is_floating_point_v<T>)
		{
			value = std::strtof(next, &end);
		}
		else
		{
			value = static_cast<T>(std::strtol(next, &end, 10));
		}
		if (end == next)
		{
			return i;
		}
		values.at(i) = value;
		next = end;
	}
	return N;
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

script_weather::LandStorm script_weather::ParseLandStorm(std::string_view shape, std::string_view clouds,
                                                         std::string_view weather)
{
	LandStorm storm;

	std::array shapeValues {storm.innerRadius, storm.outerRadius, storm.fadeSeconds,
	                        storm.lastsFor,    storm.strength,    storm.rainSpeed};
	ScanList(shape, shapeValues);
	storm.innerRadius = shapeValues[0];
	storm.outerRadius = shapeValues[1];
	storm.fadeSeconds = shapeValues[2];
	storm.lastsFor = shapeValues[3];
	storm.strength = shapeValues[4];
	storm.rainSpeed = shapeValues[5];

	std::array cloudValues {storm.cloudShade, storm.cloudHeight,   storm.boltWait.x,
	                        storm.boltWait.y, storm.thunderWait.x, storm.thunderWait.y};
	ScanList(clouds, cloudValues);
	storm.cloudShade = cloudValues[0];
	storm.cloudHeight = cloudValues[1];
	storm.boltWait = {cloudValues[2], cloudValues[3]};
	storm.thunderWait = {cloudValues[4], cloudValues[5]};

	// The weather's bytes, with one more for what spills past its end
	auto& effect = storm.effect;
	std::array<uint8_t, 9> bytes {static_cast<uint8_t>(effect.temperature),
	                              static_cast<uint8_t>(effect.rain),
	                              static_cast<uint8_t>(effect.snow),
	                              static_cast<uint8_t>(effect.overcast),
	                              static_cast<uint8_t>(effect.windX),
	                              static_cast<uint8_t>(effect.windZ),
	                              static_cast<uint8_t>(effect.snowCover),
	                              effect.stamp,
	                              0};
	// Where each number is stored: cloud, snow, temperature, rain, wind across, wind along
	constexpr std::array<size_t, 6> k_Places {3, 2, 0, 1, 4, 5};
	std::array<int32_t, 6> numbers {};
	const auto read = ScanList(weather, numbers);
	for (size_t i = 0; i < read; ++i)
	{
		const auto value = static_cast<uint32_t>(numbers.at(i));
		for (size_t b = 0; b < 4; ++b)
		{
			bytes.at(k_Places.at(i) + b) = static_cast<uint8_t>((value >> (8 * b)) & 0xffU);
		}
	}
	effect.temperature = static_cast<int8_t>(bytes[0]);
	effect.rain = static_cast<int8_t>(bytes[1]);
	effect.snow = static_cast<int8_t>(bytes[2]);
	effect.overcast = static_cast<int8_t>(bytes[3]);
	effect.windX = static_cast<int8_t>(bytes[4]);
	effect.windZ = static_cast<int8_t>(bytes[5]);
	effect.snowCover = static_cast<int8_t>(bytes[6]);
	effect.stamp = bytes[7];
	return storm;
}
