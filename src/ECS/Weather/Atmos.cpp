/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Atmos.h"

#include <algorithm>
#include <array>

#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Weather/WeatherState.h"
#include "Locator.h"
#include "Storms.h"

using namespace openblack::weather;
using namespace openblack::weather::atmos;

namespace
{
/// The weather's state (Locator::weatherSystem)
openblack::weather::State& WeatherState()
{
	return openblack::Locator::weatherSystem::value().GetState();
}

/// The cell, or the ambient weather (stamped with the current frame, so it is never recomputed)
WeatherInfo* CellAt(int32_t x, int32_t z)
{
	if (x < 0 || z < 0 || x >= k_GridSize || z >= k_GridSize)
	{
		WeatherState().ambient.stamp = WeatherState().atmosFrame;
		return &WeatherState().ambient;
	}
	return &WeatherState().atmosGrid[static_cast<size_t>(z) * k_GridSize + static_cast<size_t>(x)];
}

/// The ambient weather plus every registered storm at the cell's corner (ix x 40, 0, iz x 40)
void Recalc(WeatherInfo* cell, int32_t x, int32_t z)
{
	if (cell == nullptr || cell == &WeatherState().ambient)
	{
		return;
	}
	const glm::vec3 corner(static_cast<float>(x * 40), 0.0f, static_cast<float>(z * 40));
	WeatherInfo weather = WeatherState().ambient;
	storms::CalcAtmosAll(corner, weather);
	weather.stamp = WeatherState().atmosFrame;
	// The snow cover there x 0.5 clamped to -128..127; the snow cover is not ported
	weather.snowCover = 0;
	*cell = weather;
}

/// The height part of GetWeather / GetWeatherSmooth
void ApplyHeight(WeatherInfo& weather, float height)
{
	if (height > 50.0f)
	{
		if (height > 200.0f)
		{
			weather.temperature = WrapAdd(weather.temperature, -11); // a byte add of -11
		}
		else
		{
			weather.temperature = WrapAdd(weather.temperature, static_cast<int32_t>((height - 50.0f) * -0.075f));
		}
	}
}

/// Bytes 0..6 lerped (wrapping), the stamp of `a`
WeatherInfo Lerp(const WeatherInfo& a, const WeatherInfo& b, int32_t w)
{
	WeatherInfo result {
	    .temperature = LerpByte(a.temperature, b.temperature, w),
	    .rain = LerpByte(a.rain, b.rain, w),
	    .snow = LerpByte(a.snow, b.snow, w),
	    .overcast = LerpByte(a.overcast, b.overcast, w),
	    .windX = LerpByte(a.windX, b.windX, w),
	    .windZ = LerpByte(a.windZ, b.windZ, w),
	    .snowCover = LerpByte(a.snowCover, b.snowCover, w),
	    .stamp = a.stamp,
	};
	return result;
}
} // namespace

void atmos::Reset()
{
	storms::Clear();
	WeatherState().atmosGrid.fill(WeatherInfo {});
	WeatherState().ambient = {};
	WeatherState().atmosFrame = 1;
}

WeatherInfo atmos::GetWeather(const glm::vec3& point, bool recalc)
{
	const auto x = static_cast<int32_t>(point.x * 0.025f);
	const auto z = static_cast<int32_t>(point.z * 0.025f);
	WeatherInfo* cell = CellAt(x, z);
	if (WeatherState().atmosFrame != cell->stamp && recalc)
	{
		Recalc(cell, x, z);
	}
	WeatherInfo weather = *cell;
	ApplyHeight(weather, point.y);
	return weather;
}

WeatherInfo atmos::GetWeatherSmooth(const glm::vec3& point, bool recalc)
{
	const float fx = point.x * 0.025f;
	const float fz = point.z * 0.025f;
	const auto x = static_cast<int32_t>(fx);
	const auto z = static_cast<int32_t>(fz);
	WeatherInfo* c00 = CellAt(x, z);
	WeatherInfo* c10 = CellAt(x + 1, z);
	WeatherInfo* c01 = CellAt(x, z + 1);
	WeatherInfo* c11 = CellAt(x + 1, z + 1);
	if (recalc)
	{
		if (WeatherState().atmosFrame != c00->stamp)
		{
			Recalc(c00, x, z);
		}
		if (WeatherState().atmosFrame != c10->stamp)
		{
			Recalc(c10, x + 1, z);
		}
		if (WeatherState().atmosFrame != c01->stamp)
		{
			Recalc(c01, x, z + 1);
		}
		if (WeatherState().atmosFrame != c11->stamp)
		{
			Recalc(c11, x + 1, z + 1);
		}
	}
	const auto wx = static_cast<int32_t>((fx - static_cast<float>(x)) * 256.0f);
	const auto wz = static_cast<int32_t>((fz - static_cast<float>(z)) * 256.0f);
	const WeatherInfo row0 = Lerp(*c00, *c10, wx);
	const WeatherInfo row1 = Lerp(*c01, *c11, wx);
	WeatherInfo weather = Lerp(row0, row1, wz);
	if (point.y > 200.0f)
	{
		const int32_t k = std::min(static_cast<int32_t>((point.y - 200.0f) * 0.25f), 256);
		weather = Lerp(weather, WeatherState().ambient, k);
	}
	ApplyHeight(weather, point.y);
	return weather;
}

void atmos::UpdateGame(float visualTime, float seconds)
{
	(void)visualTime; // the sun position (visualTime x pi/12), not used here
	storms::UpdateAll(seconds);
	// The snow cover update: not ported
	if (++WeatherState().atmosFrame == 0)
	{
		for (auto& cell : WeatherState().atmosGrid)
		{
			cell.stamp = 0;
		}
		WeatherState().atmosFrame = 1;
	}
}

const WeatherInfo& atmos::Ambient()
{
	return WeatherState().ambient;
}

void atmos::SetAmbient(const WeatherInfo& ambient)
{
	WeatherState().ambient = ambient;
}

uint8_t atmos::Frame()
{
	return WeatherState().atmosFrame;
}
