/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Climate.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <numbers>

#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/MapCoords.h"
#include "Atmos.h"
#include "Calendar.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Weather/WeatherState.h"
#include "Game.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "WeatherLand.h"

using namespace openblack;
using namespace openblack::weather;
using namespace openblack::weather::climate;

namespace
{
/// the temperature's share of its min..max range by hour (0..23)
constexpr std::array<float, 24> k_HourFactor = {0.5f, 0.4f, 0.3f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f,
                                                1.0f, 1.1f, 1.2f, 1.3f, 1.5f, 1.2f, 1.1f, 1.0f, 0.9f, 0.8f, 0.7f, 0.6f};
/// by month 1..12 (February 0 as in the original; [0] is never read)
constexpr std::array<float, 13> k_MonthFactor = {0.0f, 0.1f, 0.0f, 0.3f, 0.4f, 0.5f, 0.6f, 0.8f, 1.0f, 0.7f, 0.4f, 0.3f, 0.2f};
/// The weather's state (Locator::weatherSystem)
openblack::weather::State& WeatherState()
{
	return openblack::Locator::weatherSystem::value().GetState();
}

uint32_t Turn()
{
	return game_clock::Turn();
}

const GClimateInfo* Info(int32_t index)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& climates = Locator::infoConstants::value().climate;
	return index >= 0 && static_cast<size_t>(index) < climates.size() ? &climates[static_cast<size_t>(index)] : nullptr;
}

// the four seasonal columns of GClimateInfo (spring, summer, autumn, winter)
float RainMin(const GClimateInfo* info, uint32_t season)
{
	return info != nullptr ? (&info->rainMinSpring)[season & 3] : 0.0f;
}
float RainMax(const GClimateInfo* info, uint32_t season)
{
	return info != nullptr ? (&info->rainMaxSpring)[season & 3] : 0.0f;
}
float TempMin(const GClimateInfo* info, uint32_t season)
{
	return info != nullptr ? (&info->tempMinSpring)[season & 3] : 0.0f;
}
float TempMax(const GClimateInfo* info, uint32_t season)
{
	return info != nullptr ? (&info->tempMaxSpring)[season & 3] : 0.0f;
}
float WindMin(const GClimateInfo* info, uint32_t season)
{
	return info != nullptr ? (&info->windMinSpring)[season & 3] : 0.0f;
}
float WindMax(const GClimateInfo* info, uint32_t season)
{
	return info != nullptr ? (&info->windMaxSpring)[season & 3] : 0.0f;
}

/// ScriptToVisual(VisualTime truncated toward zero), truncated toward zero again: "RelativeTime", the hour the climate's tables
/// use
int32_t RelativeHour()
{
	if (!Locator::dayNightClock::has_value())
	{
		std::fputs("weather::climate: no day/night clock in the locator (Locator::dayNightClock)\n", stderr);
		std::abort();
	}
	const auto& clock = Locator::dayNightClock::value().Clock();
	const auto visual = static_cast<float>(static_cast<int32_t>(clock.GetVisualTime()));
	return std::clamp(static_cast<int32_t>(clock.ScriptToVisual(visual)), 0, 23);
}

/// the temperature's target for the hour and the month
float TargetTemperature(float min, float max)
{
	const float hour = k_HourFactor[static_cast<size_t>(RelativeHour())];
	const auto month = static_cast<size_t>(std::clamp(calendar::GetMonth(Turn()), 1, 12));
	return hour * k_MonthFactor[month] * (max - min) + min;
}

/// the temperature goes towards the target by a tenth of the target's whole degrees each turn (so it
/// keeps oscillating around it: the Land 1 script saved 10.8 / 12)
void UpdateTemperature(Climate& climate, float min, float max)
{
	const float target = TargetTemperature(min, max);
	climate.targetTemperature = target;
	const auto step = static_cast<float>(std::abs(static_cast<int32_t>(target))) * 0.1f;
	if (target > climate.temperature)
	{
		climate.temperature += step;
	}
	else
	{
		climate.temperature -= step;
	}
}

/// the climate's rain update, once per game day: while falling only the days count; else the desire grows by two random
/// shares, of the season's rainMin x 0.02 and of its rainMax x 0.02 (the month, hour and nature terms of
/// the climate rain info are 0: that table is not in info.dat)
void UpdateRain(Rain& rain, float min, float max)
{
	if ((rain.flags & 1) != 0)
	{
		++rain.rainingDays;
		return;
	}
	++rain.dryDays;
	if (--rain.rainingDays < 0)
	{
		rain.rainingDays = 0;
	}
	constexpr float k_MonthRain = 0.0f;  // the rain info's month[m]
	constexpr float k_HourRain = 0.0f;   // .hour[RelativeTime]
	constexpr float k_NatureRain = 0.0f; // .natureRainDesire
	// the first share from rainMin, the second from rainMax; 0.02 is a double, the product rounded once to float
	const float r1 = GameFloatRand(static_cast<float>(static_cast<double>(min) * 0.02));
	const float r2 = GameFloatRand(static_cast<float>(static_cast<double>(max) * 0.02));
	rain.desire = r2 + r1 + (k_NatureRain + 1.0f) * rain.desire + k_HourRain + k_MonthRain;
	if (rain.desire > 1.0f)
	{
		rain.desire = 1.0f;
	}
}

/// once per game day: the wind from the season's range along windAngle. The weight is
/// int(exp(-((rain - 0.5) x 5)^2)), 1 only at a desire of exactly 0.5, so it is min + max almost always.
void UpdateWind(Climate& climate, float min, float max, float rain)
{
	const float t = (rain - 0.5f) * 5.0f;
	const auto k = static_cast<float>(static_cast<int32_t>(std::exp(-(t * t))));
	const float c = std::cos(climate.windAngle);
	const float s = std::sin(climate.windAngle);
	const float ax = min * c;
	const float bx = max * c;
	climate.windX = bx - (bx - ax) * k + ax;
	const float az = min * s;
	const float bz = max * s;
	climate.windZ = bz - (bz - az) * k + az;
}

/// out of the list, its storms deleted
void Delete(Climate* climate)
{
	for (auto id : climate->storms)
	{
		storms::Destroy(id);
	}
	if (WeatherState().worldClimate == climate)
	{
		WeatherState().worldClimate = nullptr;
	}
	WeatherState().climates.remove_if([climate](const Climate& c) { return &c == climate; });
}

/// what the local and the world climate's construction share: the season's rain and temperature, the storm defaults
void InitFromInfo(Climate& climate)
{
	const auto* info = Info(climate.info);
	const uint32_t season = calendar::GetSeason(Turn());
	// the desire starts at rainMax and the raining days at int(rainMin x 100)
	climate.rain = {};
	climate.rain.desire = RainMax(info, season);
	climate.rain.rainingDays = static_cast<int32_t>(RainMin(info, season) * 100.0f);
	// the target for tempMin, tempMax
	climate.temperature = TargetTemperature(TempMin(info, season), TempMax(info, season));
	climate.targetTemperature = climate.temperature;
	// no wind until the script sets it
	climate.fallSpeed = static_cast<float>(static_cast<double>(WindMax(info, WeatherState().season)) * (1.0 / 30.0) + 0.5);
}

/// the world's anywhere (a random 10 m cell of the 512); a local climate's at
/// r^2 x innerRadius (r random 0..1) in a random direction. It retries (20 times at most) while the place is not in any
/// climate's radius and IsWater returns 1 (only off the land: see IsOffMapOrNoLand).
glm::vec3 FindWhereToCreateStorm(const Climate& climate)
{
	glm::vec3 place(0.0f);
	for (int32_t tries = 0;;)
	{
		bool outside = true;
		if (!climate.world)
		{
			const float angle = GameFloatRand(std::numbers::pi_v<float> * 2.0f);
			const float r = GameFloatRand(1.0f);
			const float distance = r * r * climate.innerRadius * 0.1f; // in 10 m cells
			// the centre's cell is the unsigned high word of its MapCoords (map_coords::CellOf); it is added as an
			// integer and the sum truncated
			const float cx = static_cast<float>(map_coords::CellOf(climate.x));
			const float cz = static_cast<float>(map_coords::CellOf(climate.z));
			place.x = static_cast<float>(static_cast<int32_t>(std::cos(angle) * distance + cx)) * 10.0f;
			place.z = static_cast<float>(static_cast<int32_t>(std::sin(angle) * distance + cz)) * 10.0f;
		}
		else
		{
			place.x = static_cast<float>(GameRand(0x200)) * 10.0f;
			place.z = static_cast<float>(GameRand(0x200)) * 10.0f;
			for (const auto& other : WeatherState().climates)
			{
				const auto centre = other.Centre();
				// the 2D distance with the table root (gutils::Hypotenuse)
				if (gutils::Hypotenuse(place.x - centre.x, place.z - centre.z) < other.outerRadius)
				{
					outside = false;
					break;
				}
			}
		}
		++tries;
		if (!outside || tries >= 20 || !IsOffMapOrNoLand(place.x, place.z))
		{
			break;
		}
	}
	return place;
}
} // namespace

glm::vec3 Climate::Centre() const
{
	return {map_coords::ToMetres(x), y, map_coords::ToMetres(z)}; // MapCoords: x 10 / 65536
}

glm::vec3 Climate::CellCentre() const
{
	// unsigned high word * 10, then to float
	return {static_cast<float>(map_coords::CellOf(x) * 10), y, static_cast<float>(map_coords::CellOf(z) * 10)};
}

void climate::Reset()
{
	for (const auto& c : WeatherState().climates)
	{
		for (auto id : c.storms)
		{
			storms::Destroy(id);
		}
	}
	WeatherState().climates.clear();
	WeatherState().worldClimate = nullptr;
	WeatherState().climateSystem = true;
	WeatherState().stormCreation = true;
	WeatherState().lastDay = 0;
	WeatherState().lastSeason = 0;
	WeatherState().season = 0;
	WeatherState().nextClimateId = 1;
}

Climate& climate::Create(const glm::vec3& position, int32_t info, float radius1, float radius2, float angle, int32_t id)
{
	Climate climate;
	if (id == 0)
	{
		// the old world goes first; centre (2560, 2560) (words 0x100), radius 5120, WORLD
		if (WeatherState().worldClimate != nullptr)
		{
			Delete(WeatherState().worldClimate);
		}
		climate.id = 0;
		climate.info = 0;
		climate.x = 0x01000000;
		climate.z = 0x01000000;
		climate.y = 0.0f;
		climate.innerRadius = 5120.0f;
		climate.outerRadius = 5120.0f;
		climate.world = true;
		climate.maxStorms = 10;
		InitFromInfo(climate);
	}
	else
	{
		// a local climate
		climate.x = map_coords::ToFixed(position.x); // MapCoords from metres, truncated
		climate.z = map_coords::ToFixed(position.z);
		climate.y = position.y;
		climate.info = info;
		climate.innerRadius = radius1 <= radius2 ? radius1 : radius2;
		climate.outerRadius = radius1 <= radius2 ? radius2 : radius1;
		InitFromInfo(climate);
		climate.windAngle = angle;
		climate.maxStorms = static_cast<int32_t>(radius2 * 0.001f + 1.0f);
		climate.world = false;
		// the id: the counter follows the script's ids
		if (WeatherState().nextClimateId == id)
		{
			++WeatherState().nextClimateId;
		}
		else if (WeatherState().nextClimateId < id)
		{
			WeatherState().nextClimateId = id + 1;
		}
		climate.id = id;
	}
	WeatherState().climates.push_front(std::move(climate));
	auto& created = WeatherState().climates.front();
	if (created.world)
	{
		WeatherState().worldClimate = &created;
	}
	return created;
}

Climate* climate::Find(int32_t id)
{
	for (auto& c : WeatherState().climates)
	{
		if (c.id == id)
		{
			return &c;
		}
	}
	return nullptr;
}

Climate& climate::World()
{
	if (WeatherState().worldClimate == nullptr)
	{
		Create(glm::vec3(0.0f), 0, 0.0f, 0.0f, 0.0f, 0);
	}
	return *WeatherState().worldClimate;
}

bool climate::HasWorld()
{
	return WeatherState().worldClimate != nullptr;
}

void climate::SetRain(int32_t id, const Rain& rain)
{
	Climate* c = id == 0 ? &World() : Find(id);
	if (c != nullptr)
	{
		c->rain = rain;
	}
}

void climate::SetTemperature(int32_t id, float temperature, float target)
{
	Climate* c = id == 0 ? WeatherState().worldClimate : Find(id); // reads the world without making it
	if (c != nullptr)
	{
		c->temperature = temperature;
		c->targetTemperature = target;
	}
}

void climate::SetWind(int32_t id, float windX, float windZ, float angle)
{
	Climate* c = id == 0 ? &World() : Find(id);
	if (c != nullptr)
	{
		c->windX = windX;
		c->windZ = windZ;
		c->windAngle = angle;
	}
}

void climate::CreateScriptStorm(int32_t id, const storms::StormDescriptor& descriptor, float age, float speed,
                                const glm::vec3& target)
{
	Climate* c = Find(id);
	if (c == nullptr)
	{
		return;
	}
	const auto stormId = storms::Create(descriptor);
	if (auto* storm = storms::Find(stormId))
	{
		storm->age = age;
		storm->speed = speed;
		storm->target = target;
	}
	c->storms.push_front(stormId);
}

weather::WeatherInfo climate::ComputeWeather(const glm::vec3& point, bool smooth)
{
	const auto& world = World();
	const weather::WeatherInfo grid = smooth ? atmos::GetWeatherSmooth(point, true) : atmos::GetWeather(point, true);

	// the world's own temperature and wind first ...
	auto temperature = static_cast<int8_t>(static_cast<int32_t>(world.temperature));
	auto windX = static_cast<int8_t>(static_cast<int32_t>(world.windX + 0.0f));
	auto windZ = static_cast<int8_t>(static_cast<int32_t>(world.windZ + 0.0f));
	// ... then every climate in the list, the world again included, weighted by the distance (1 inside the inner
	// radius, 0 at the outer)
	for (const auto& c : WeatherState().climates)
	{
		const auto centre = c.Centre();
		const float r = c.outerRadius;
		if (!(centre.x - r <= point.x) || centre.x + r < point.x || !(centre.z - r <= point.z) || centre.z + r < point.z)
		{
			continue;
		}
		const float dx = point.x - centre.x;
		const float dz = point.z - centre.z;
		const float d2 = dx * dx + dz * dz;
		if (d2 > r * r)
		{
			continue;
		}
		float f = d2 <= c.innerRadius * c.innerRadius ? 1.0f : 1.0f - (std::sqrt(d2) - c.innerRadius) / (r - c.innerRadius);
		f = f <= 0.0f ? 0.0f : (f < 1.0f ? f : 1.0f);
		const auto t = static_cast<int8_t>(static_cast<int32_t>(c.temperature));
		const auto wx = static_cast<int8_t>(static_cast<int32_t>(c.windX));
		const auto wz = static_cast<int8_t>(static_cast<int32_t>(c.windZ));
		temperature = static_cast<int8_t>(static_cast<int32_t>(static_cast<float>(t) * f + static_cast<float>(temperature)));
		windX = static_cast<int8_t>(static_cast<int32_t>(static_cast<float>(wx) * f + static_cast<float>(windX)));
		windZ = static_cast<int8_t>(static_cast<int32_t>(static_cast<float>(wz) * f + static_cast<float>(windZ)));
	}

	const auto resultTemperature = ClampAdd(grid.temperature, temperature);
	const auto resultWindX = ClampAdd(grid.windX, windX);
	const auto resultWindZ = ClampAdd(grid.windZ, windZ);
	const auto resultSnowCover = ClampAdd(grid.snowCover, 0);
	const auto resultSnow = ClampAdd(0, grid.snow);
	const auto resultRain = ClampAdd(grid.rain, 0);
	const auto resultOvercast = ClampAdd(0, grid.overcast);
	weather::WeatherInfo result {
	    .temperature = resultTemperature,
	    .rain = resultRain,
	    .snow = resultSnow,
	    .overcast = resultOvercast,
	    .windX = resultWindX,
	    .windZ = resultWindZ,
	    .snowCover = resultSnowCover,
	    .stamp = 0,
	};
	return result;
}

void climate::CreateStorm(Climate& climate, uint32_t turn)
{
	(void)turn;
	const auto* info = Info(climate.info);
	const float rainMax = RainMax(info, WeatherState().season);
	const float rainMin = RainMin(info, WeatherState().season);
	const auto days = static_cast<float>(climate.rain.rainingDays);
	// rained enough this season: the desire starts again
	if (!(rainMax > days * 0.01f))
	{
		climate.rain.desire = 0.0f;
		climate.rain.dryDays = 0;
		return;
	}
	if (static_cast<int32_t>(climate.storms.size()) >= climate.maxStorms)
	{
		return;
	}
	const glm::vec3 place = FindWhereToCreateStorm(climate);
	uint32_t size;
	if (climate.world)
	{
		size = GameRand(1000);
	}
	else
	{
		const auto centre = climate.Centre();
		// the 2D distance, then truncated
		size = static_cast<uint32_t>(static_cast<int32_t>(gutils::Hypotenuse(place.x - centre.x, place.z - centre.z)));
	}
	size = std::clamp<uint32_t>(size, 160, 900);

	storms::StormDescriptor d;
	// the 10 m cell at a height of 300
	d.position = glm::vec3(place.x, 300.0f, place.z);
	d.innerRadius = static_cast<float>(size);
	d.outerRadius = static_cast<float>(static_cast<int32_t>(static_cast<double>(size) * 1.1));
	d.fadeInTime = 10.0f;
	d.lifeTime = rainMax * 10.0f * 10.0f;
	if (!(rainMin < days * 0.01f))
	{
		d.lifeTime = (rainMax * 100.0f - days) * 10.0f;
	}
	if (d.lifeTime < 20.0f)
	{
		d.lifeTime = 20.0f;
	}
	d.strength = 1.0f;
	const auto temperature = static_cast<int8_t>(static_cast<int32_t>(climate.temperature));
	d.elevation = static_cast<float>(climate.stormElevation);
	d.fallSpeed = climate.fallSpeed;
	d.weather = {};
	d.weather.temperature = temperature;
	// blackness exp(-((t - 30) / 15)^2); above 30 degrees (or with `lightning`) a thunderstorm with the climate's lightning
	const float x = static_cast<float>(temperature);
	const auto u = static_cast<float>((static_cast<double>(x) - 30.0) * (1.0 / 15.0));
	d.blackness = std::exp(-(u * u));
	if (x > 30.0f || climate.lightning != 0)
	{
		d.sheetMin = climate.sheetMin;
		d.sheetMax = climate.sheetMax;
		d.forkMax = climate.forkMax;
		d.forkMin = climate.forkMin;
		d.blackness = static_cast<float>(climate.lightning);
	}
	d.weather.overcast = static_cast<int8_t>(static_cast<int32_t>(d.blackness * 100.0f));
	if (temperature < 0)
	{
		d.weather.snow = 100;
		d.weather.rain = 0;
	}
	else
	{
		// snow share exp(-(t x 0.2)^2): all rain above about 10 degrees
		const auto v = static_cast<float>(static_cast<double>(x) * 0.2);
		const auto snow = static_cast<int32_t>(static_cast<double>(std::exp(-(v * v))) * 100.0);
		d.weather.snow = static_cast<int8_t>(snow);
		d.weather.rain = static_cast<int8_t>(100 - snow);
	}
	d.weather.windX = static_cast<int8_t>(static_cast<int32_t>(climate.windX));
	d.weather.windZ = static_cast<int8_t>(static_cast<int32_t>(climate.windZ));
	if (d.lifeTime > 0.0f)
	{
		if (d.lifeTime < 8.0f) // below 8, then 20
		{
			d.lifeTime = 20.0f;
		}
		const auto id = storms::Create(d);
		climate.storms.push_front(id);
		climate.rain.flags |= 1;
		// (the original's debug message; there is no "game" logger in the unit tests)
		if (auto logger = spdlog::get("game"); logger)
		{
			SPDLOG_LOGGER_INFO(logger,
			                   "Weather: climate {} makes a storm at ({:.0f}, {:.0f}) r {:.0f}/{:.0f}, life {:.0f} s, "
			                   "{} deg, rain {} snow {}",
			                   climate.id, d.position.x, d.position.z, d.innerRadius, d.outerRadius, d.lifeTime,
			                   static_cast<int>(d.weather.temperature), static_cast<int>(d.weather.rain),
			                   static_cast<int>(d.weather.snow));
		}
	}
	// the desire starts again
	climate.rain.desire = 0.0f;
	climate.rain.dryDays = 0;
}

namespace
{
/// one climate's turn
void ProcessClimate(Climate& climate, bool newDay, uint32_t turn)
{
	(void)climate::World();
	const auto* info = Info(climate.info);
	if (WeatherState().climateSystem)
	{
		UpdateTemperature(climate, TempMin(info, WeatherState().season), TempMax(info, WeatherState().season));
	}

	// its storms: forget the gone ones; the rest drift with the atmosphere's wind (x 0.01 m per turn) and, on a new
	// day, start fading when they left the climate (a world storm: when it is in any climate), or when it rained enough
	auto it = climate.storms.begin();
	while (it != climate.storms.end())
	{
		storms::Storm* storm = storms::Find(*it);
		if (storm == nullptr)
		{
			const auto gone = *it;
			climate.storms.remove(gone);
			if (climate.storms.empty())
			{
				climate.rain.flags &= ~1;
				break;
			}
			// the original goes on after the list's head
			it = std::next(climate.storms.begin());
			continue;
		}
		auto& d = storm->descriptor;
		const weather::WeatherInfo w = atmos::GetWeather(d.position, true);
		d.position.x += static_cast<float>(w.windX) * 0.01f;
		d.position.z += static_cast<float>(w.windZ) * 0.01f;
		if (newDay)
		{
			const float fadeOutAge = d.lifeTime - (d.fadeInTime + d.fadeInTime);
			if (climate.world)
			{
				for (const auto& other : WeatherState().climates)
				{
					const auto centre = other.CellCentre();
					if (gutils::Hypotenuse(d.position.x - centre.x, d.position.z - centre.z) < other.outerRadius &&
					    fadeOutAge > storm->age)
					{
						storm->age = fadeOutAge;
						climate.rain.desire = 1.0f;
						break;
					}
				}
			}
			else
			{
				const auto centre = climate.CellCentre();
				if (gutils::Hypotenuse(d.position.x - centre.x, d.position.z - centre.z) > climate.outerRadius &&
				    fadeOutAge > storm->age)
				{
					storm->age = fadeOutAge;
					climate.rain.desire = 1.0f;
				}
			}
			if (RainMax(info, WeatherState().season) < static_cast<float>(climate.rain.rainingDays) * 0.01f &&
			    d.lifeTime - (d.fadeInTime + d.fadeInTime) > storm->age)
			{
				storm->age = d.lifeTime - (d.fadeInTime + d.fadeInTime);
			}
		}
		++it;
	}

	if (newDay && WeatherState().climateSystem)
	{
		UpdateRain(climate.rain, RainMin(info, WeatherState().season), RainMax(info, WeatherState().season));
		UpdateWind(climate, WindMin(info, WeatherState().season), WindMax(info, WeatherState().season), climate.rain.desire);
		if (climate.rain.desire == 1.0f && WeatherState().stormCreation)
		{
			CreateStorm(climate, turn);
		}
	}
}
} // namespace

void climate::ProcessAll(uint32_t turn)
{
	(void)World();
	// (the debug messages about the climate at the hand are skipped)
	bool newDay = false;
	const auto day = static_cast<int32_t>(calendar::GetDayOfMonth(turn));
	if (WeatherState().lastDay != day)
	{
		newDay = true;
		WeatherState().lastDay = day;
		const uint32_t season = calendar::GetSeason(turn);
		if (WeatherState().lastSeason != season)
		{
			WeatherState().season = season;
			WeatherState().lastSeason = season;
		}
	}
	for (auto& c : WeatherState().climates)
	{
		ProcessClimate(c, newDay, turn);
	}
}

void climate::SetClimateSystemEnabled(bool on)
{
	WeatherState().climateSystem = on;
}

void climate::SetStormCreationEnabled(bool on)
{
	WeatherState().stormCreation = on;
}

bool climate::IsClimateSystemEnabled()
{
	return WeatherState().climateSystem;
}

bool climate::IsStormCreationEnabled()
{
	return WeatherState().stormCreation;
}

uint32_t climate::CurrentSeason()
{
	return WeatherState().season;
}

void climate::ForEach(const std::function<void(const Climate&)>& function)
{
	for (const auto& c : WeatherState().climates)
	{
		function(c);
	}
}
