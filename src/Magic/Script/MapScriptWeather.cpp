/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MapScriptWeather.h"

#include <cstdio>
#include <cstring>

#include <array>

#include "ECS/Weather/Climate.h"
#include "ECS/Weather/Storms.h"

using namespace openblack;
using namespace openblack::weather;

void magic::map_script::CreateWeatherClimate(int32_t id, int32_t info, const glm::vec3& position, float radius1, float radius2)
{
	// the script position: a MapCoords, y 0
	climate::Create(glm::vec3(position.x, 0.0f, position.z), info, radius1, radius2, 0.0f, id);
}

void magic::map_script::CreateWeatherClimateRain(int32_t id, float desire, int32_t dryDays, int32_t rainingDays, int32_t flags)
{
	climate::Rain rain {
	    .desire = desire,
	    .dryDays = dryDays,
	    .rainingDays = rainingDays,
	    .flags = static_cast<uint8_t>(flags),
	};
	climate::SetRain(id, rain);
}

void magic::map_script::CreateWeatherClimateTemp(int32_t id, float temperature, float target)
{
	climate::SetTemperature(id, temperature, target);
}

void magic::map_script::CreateWeatherClimateWind(int32_t id, float windX, float windZ, float angle)
{
	climate::SetWind(id, windX, windZ, angle);
}

void magic::map_script::CreateWeatherStorm(int32_t climateId, const glm::vec3& position, float age, int32_t numClouds,
                                           const std::string& shape, const std::string& clouds, const std::string& weather,
                                           float speed, const glm::vec3& target)
{
	// a storm descriptor at (x, land + 0, z)
	storms::StormDescriptor d {
	    .position = position,
	    .innerRadius = 0.0f,
	    .outerRadius = 0.0f,
	    .numClouds = numClouds,
	};
	std::array<float, 6> a {d.innerRadius, d.outerRadius, d.fadeInTime, d.lifeTime, d.strength, d.fallSpeed};
	// NOLINTNEXTLINE(cert-err34-c): sscanf, as the original
	std::sscanf(shape.c_str(), "%f,%f,%f,%f,%f,%f", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]);
	d.innerRadius = a[0];
	d.outerRadius = a[1];
	d.fadeInTime = a[2];
	d.lifeTime = a[3];
	d.strength = a[4];
	d.fallSpeed = a[5];
	std::array<float, 6> b {d.blackness, d.elevation, d.forkMin, d.forkMax, d.sheetMin, d.sheetMax};
	// NOLINTNEXTLINE(cert-err34-c)
	std::sscanf(clouds.c_str(), "%f,%f,%f,%f,%f,%f", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
	d.blackness = b[0];
	d.elevation = b[1];
	d.forkMin = b[2];
	d.forkMax = b[3];
	d.sheetMin = b[4];
	d.sheetMax = b[5];
	// "%d" x 6 into byte pointers: 4-byte writes at bytes 3, 2, 0, 1, 4 and 5 of the weather values, in that order (the
	// last one reaches past the descriptor: a stack slot of the handler)
	std::array<uint8_t, 9> bytes {};
	std::memcpy(bytes.data(), &d.weather, 8);
	std::array<int32_t, 6> v {};
	// NOLINTNEXTLINE(cert-err34-c)
	const int read = std::sscanf(weather.c_str(), "%d,%d,%d,%d,%d,%d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);
	constexpr std::array<size_t, 6> k_Offsets = {3, 2, 0, 1, 4, 5};
	for (int i = 0; i < read && i < 6; ++i)
	{
		std::memcpy(bytes.data() + k_Offsets[static_cast<size_t>(i)], &v[static_cast<size_t>(i)], 4);
	}
	std::memcpy(&d.weather, bytes.data(), 8);

	climate::CreateScriptStorm(climateId, d, age, speed, target);
}
