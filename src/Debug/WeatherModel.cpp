/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WeatherModel.h"

#include <cmath>

#include <algorithm>
#include <numbers>

namespace openblack::debug::weather_window
{

glm::i8vec2 WindBytes(float degrees, int strength)
{
	const float radians = degrees * std::numbers::pi_v<float> / 180.0f;
	const auto byte = [](float value) { return static_cast<int8_t>(std::clamp<long>(std::lround(value), -128, 127)); };
	return {byte(std::sin(radians) * static_cast<float>(strength)), byte(std::cos(radians) * static_cast<float>(strength))};
}

void ApplyPreset(const Preset& preset, StormSettings& settings)
{
	settings.rain = preset.rain;
	settings.overcast = preset.overcast;
	settings.temperature = preset.temperature;
	settings.windStrength = preset.windStrength;
	settings.lightning = preset.lightning;
	settings.thunderWait = preset.lightning ? k_ThunderWait : glm::vec2(0.0f);
	settings.boltWait = preset.lightning ? k_BoltWait : glm::vec2(0.0f);
}

void SetLightning(StormSettings& settings, bool on)
{
	settings.lightning = on;
	if (!on)
	{
		settings.thunderWait = glm::vec2(0.0f);
		settings.boltWait = glm::vec2(0.0f);
	}
	else if (settings.thunderWait.y == 0.0f)
	{
		settings.thunderWait = k_ThunderWait;
		settings.boltWait = k_BoltWait;
	}
}

weather::storms::StormDescriptor IslandStorm(const StormSettings& settings)
{
	weather::storms::StormDescriptor descriptor;
	descriptor.position = {k_IslandCentre.x, 0.0f, k_IslandCentre.y};
	descriptor.innerRadius = k_IslandInnerRadius;
	descriptor.outerRadius = k_IslandOuterRadius;
	descriptor.fadeInTime = settings.fadeSeconds;
	descriptor.lifeTime = settings.seconds;
	descriptor.strength = 1.0f;
	descriptor.numClouds = 0;
	descriptor.elevation = settings.cloudHeight;
	descriptor.fallSpeed = settings.rainSpeed;
	descriptor.sheetMin = settings.lightning ? settings.thunderWait.x : 0.0f;
	descriptor.sheetMax = settings.lightning ? settings.thunderWait.y : 0.0f;
	descriptor.forkMin = settings.lightning ? settings.boltWait.x : 0.0f;
	descriptor.forkMax = settings.lightning ? settings.boltWait.y : 0.0f;
	const auto wind = WindBytes(settings.windDegrees, settings.windStrength);
	descriptor.weather = {};
	descriptor.weather.temperature = settings.temperature;
	descriptor.weather.rain = settings.rain;
	descriptor.weather.overcast = settings.overcast;
	descriptor.weather.windX = wind.x;
	descriptor.weather.windZ = wind.y;
	return descriptor;
}

StormPhase PhaseOf(const weather::storms::Storm& storm)
{
	if (storm.deleteCounter != 0)
	{
		return StormPhase::Ending;
	}
	const auto& descriptor = storm.descriptor;
	if (storm.age >= descriptor.fadeInTime && descriptor.lifeTime - storm.age < descriptor.fadeInTime)
	{
		return StormPhase::Clearing;
	}
	return StormPhase::Live;
}

bool HasLightning(const weather::storms::StormDescriptor& descriptor)
{
	return descriptor.sheetMax != 0.0f || descriptor.forkMax != 0.0f;
}

int LyingPercent(int8_t snowCover)
{
	return std::max<int>(snowCover, 0) * 100 / 127;
}

int FlashPercent(uint8_t flash)
{
	return static_cast<int>(flash) * 100 / 255;
}

} // namespace openblack::debug::weather_window
