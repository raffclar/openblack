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

#include "Common/GUtilsDistance.h"

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
	const auto weather = ClimateWeather({.windX = air.x, .windZ = air.z}, global, climates, point);
	return {.x = weather.windX, .z = weather.windZ};
}

components::WeatherInfo script_weather::ClimateWeather(components::WeatherInfo air, const ClimateWind& global,
                                                       std::span<const ClimateWind> climates, glm::vec2 point)
{
	// The part of the climates. Each step cuts the value to a whole number and keeps it to a byte.
	int8_t temperature = WholeByte(global.temperature);
	int8_t windX = WholeByte(global.windX);
	int8_t windZ = WholeByte(global.windZ);
	for (const auto& climate : climates)
	{
		const auto share = ClimateShare(climate, point);
		if (!share.has_value())
		{
			continue;
		}
		const auto add = [share](float value, int8_t sum) {
			const float part = (static_cast<float>(WholeByte(value)) * *share) + static_cast<float>(sum);
			return static_cast<int8_t>(static_cast<uint8_t>(static_cast<int32_t>(part) & 0xff));
		};
		temperature = add(climate.temperature, temperature);
		windX = add(climate.windX, windX);
		windZ = add(climate.windZ, windZ);
	}
	const auto sum = [](int8_t a, int8_t b) {
		return static_cast<int8_t>(std::clamp(static_cast<int32_t>(a) + static_cast<int32_t>(b), -128, 127));
	};
	air.temperature = sum(air.temperature, temperature);
	air.windX = sum(air.windX, windX);
	air.windZ = sum(air.windZ, windZ);
	return air;
}

glm::vec3 script_weather::Drift(glm::vec3 position, Wind wind)
{
	const auto along = [](int8_t byte, float from) { return (static_cast<float>(byte) * k_WindDrift) + from; };
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

void script_weather::EndStorm(components::Storm& storm)
{
	storm.dead = true;
}

script_weather::ThingAfterTurn script_weather::AfterTurn(bool stormLives, bool heldByScript)
{
	if (stormLives)
	{
		return ThingAfterTurn::FollowsItsStorm;
	}
	return heldByScript ? ThingAfterTurn::StaysWithoutStorm : ThingAfterTurn::Goes;
}

float script_weather::ThingSpeed(const components::Storm* storm)
{
	return storm != nullptr ? storm->speed : 0.0f;
}

bool script_weather::InKillArea(glm::vec3 stormPlace, float stormOuterRadius, glm::vec3 centre, float radius)
{
	// Each step is a float, in the order of the original game
	const float acrossX = stormPlace.x - centre.x;
	const float acrossZ = stormPlace.z - centre.z;
	const float distance = std::sqrt((acrossZ * acrossZ) + (acrossX * acrossX));
	return distance < radius + stormOuterRadius;
}

bool script_weather::WindMagicNear(glm::vec3 stormPlace, float stormOuterRadius, int32_t cloudCount, glm::vec3 point,
                                   float radius)
{
	if (cloudCount != 0)
	{
		return false;
	}
	// The radius of the question comes first in the sum, then the outer radius of the storm
	return radius + stormOuterRadius > gutils::GetDistance(point, stormPlace);
}
