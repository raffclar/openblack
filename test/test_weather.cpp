/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "Common/GameRandomTesting.h"
#include "ECS/Systems/Implementations/DayNightClockSystem.h"
#include "ECS/Weather/Atmos.h"
#include "ECS/Weather/Calendar.h"
#include "ECS/Weather/Climate.h"
#include "ECS/Weather/Storms.h"
#include "ECS/Weather/Weather.h"
#include "ECS/Weather/WeatherInfo.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Script/MapScriptWeather.h"

using namespace openblack;
using namespace openblack::weather;

namespace
{
class WeatherTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto info = std::make_unique<InfoConstants>();
		auto& world = info->climate.at(0);
		world.rainMinSpring = 0.1f;
		world.rainMaxSpring = 0.3f;
		world.tempMinSpring = 12.0f;
		world.tempMaxSpring = 12.0f;
		world.windMinSpring = 10.0f;
		world.windMaxSpring = 20.0f;
		Locator::infoConstants::reset(info.release());
		// the game's day / night clock as it starts: noon (the climate's hour reads it)
		Locator::dayNightClock::emplace<ecs::systems::DayNightClockSystem>();
		climate::Reset();
		atmos::Reset();
	}

	void TearDown() override
	{
		climate::Reset();
		atmos::Reset();
		Locator::dayNightClock::reset();
		Locator::infoConstants::reset();
	}

	/// A storm like the storm miracle's descriptor, at full strength after two seconds
	static storms::StormId MakeStorm(const glm::vec3& position)
	{
		storms::StormDescriptor d;
		d.position = position;
		d.innerRadius = 50.0f;
		d.outerRadius = 100.0f;
		d.fadeInTime = 1.0f;
		d.lifeTime = 1000.0f;
		d.weather = {};
		d.weather.temperature = 20;
		d.weather.rain = 100;
		d.weather.overcast = 80;
		const auto id = storms::Create(d);
		for (int i = 0; i < 20; ++i)
		{
			atmos::UpdateGame(12.0f, 0.1f);
		}
		return id;
	}
};
} // namespace

TEST(WeatherBytes, WrapClampLerp)
{
	EXPECT_EQ(WrapAdd(120, 10), -126);
	EXPECT_EQ(ClampAdd(120, 10), 127);
	EXPECT_EQ(ClampAdd(-120, -10), -128);
	EXPECT_EQ(LerpByte(10, 20, 128), 15);
	EXPECT_EQ(LerpByte(20, 10, 128), 15);
	EXPECT_EQ(LerpByte(0, 20, 256), 20);
}

TEST(WeatherCalendar, StartDateAndSeasons)
{
	// 5 May 1998 18:05:30: 120 days of January..April, the 5th (added whole), 0.754 of a day
	EXPECT_NEAR(calendar::GetDayOfYear(0), 125.754f, 0.001f);
	EXPECT_EQ(calendar::GetMonth(0), 5);
	EXPECT_NEAR(calendar::GetDayOfMonth(0), 5.754f, 0.001f);
	EXPECT_EQ(calendar::GetSeason(0), 0u); // spring
	// one day every 36000 / 365.25 = 98.56 turns
	EXPECT_NEAR(calendar::GetDayOfYear(99) - calendar::GetDayOfYear(0), 99.0f / 98.5626f, 0.001f);
	// summer from day 171: (171 - 125.754) x 98.5626 = 4459.6 turns
	EXPECT_EQ(calendar::GetSeason(4459), 0u);
	EXPECT_EQ(calendar::GetSeason(4460), 1u);
	// winter before day 79 of the next year
	EXPECT_EQ(calendar::GetSeason(static_cast<uint32_t>((365.25f - 125.754f + 10.0f) * 98.5626f)), 3u);
	// autumn from day 263, winter again from day 354 (GetSeason's fourth start)
	EXPECT_EQ(calendar::GetSeason(static_cast<uint32_t>((300.0f - 125.754f) * 98.5626f)), 2u);
	EXPECT_EQ(calendar::GetSeason(static_cast<uint32_t>((360.0f - 125.754f) * 98.5626f)), 3u);
}

TEST_F(WeatherTest, StormFadeAndCalcAtmos)
{
	const auto id = MakeStorm({1000.0f, 0.0f, 1000.0f});
	const auto* storm = storms::Find(id);
	ASSERT_NE(storm, nullptr);
	EXPECT_FLOAT_EQ(storm->fade, 1.0f);
	EXPECT_FLOAT_EQ(storm->innerRadius, 50.0f);
	EXPECT_FLOAT_EQ(storm->outerRadius, 100.0f);

	weather::WeatherInfo w {};
	storms::CalcAtmos(*storm, {1000.0f, 0.0f, 1000.0f}, w);
	EXPECT_EQ(w.temperature, 20);
	EXPECT_EQ(w.rain, 100);
	EXPECT_EQ(w.overcast, 80);
	// half way between the inner and the outer radius: w = 128
	w = {};
	storms::CalcAtmos(*storm, {1075.0f, 0.0f, 1000.0f}, w);
	EXPECT_EQ(w.rain, 50);
	EXPECT_EQ(w.temperature, 10);
	// the outer radius and past it: nothing
	w = {};
	storms::CalcAtmos(*storm, {1100.0f, 0.0f, 1000.0f}, w);
	EXPECT_EQ(w.rain, 0);
	storms::CalcAtmos(*storm, {1101.0f, 0.0f, 1000.0f}, w);
	EXPECT_EQ(w.rain, 0);
}

TEST_F(WeatherTest, StormFadesInOverFadeInTime)
{
	storms::StormDescriptor d;
	d.position = {500.0f, 0.0f, 500.0f};
	d.fadeInTime = 10.0f;
	d.innerRadius = 100.0f;
	const auto id = storms::Create(d);
	for (int i = 0; i < 50; ++i)
	{
		atmos::UpdateGame(12.0f, 0.1f);
	}
	const auto* storm = storms::Find(id);
	ASSERT_NE(storm, nullptr);
	EXPECT_NEAR(storm->fade, 0.5f, 1e-4f);
	EXPECT_NEAR(storm->innerRadius, 50.0f, 1e-2f);
}

TEST_F(WeatherTest, AtmosGridAndHeight)
{
	MakeStorm({1000.0f, 0.0f, 1000.0f});
	// the cell's corner (1000, 1000) is the storm's centre
	auto w = atmos::GetWeather({1010.0f, 0.0f, 1030.0f});
	EXPECT_EQ(w.rain, 100);
	EXPECT_EQ(w.temperature, 20);
	EXPECT_EQ(w.stamp, atmos::Frame());
	// above 50 m the temperature drops by 0.075 per metre, 11 above 200 m
	EXPECT_EQ(atmos::GetWeather({1010.0f, 100.0f, 1030.0f}).temperature, 17); // 20 + trunc(-3.75)
	EXPECT_EQ(atmos::GetWeather({1010.0f, 250.0f, 1030.0f}).temperature, 9);
	// outside the grid: the ambient weather (0)
	EXPECT_EQ(atmos::GetWeather({-100.0f, 0.0f, 1000.0f}).rain, 0);
	EXPECT_EQ(atmos::GetWeather({6000.0f, 0.0f, 1000.0f}).rain, 0);
}

TEST_F(WeatherTest, FallingSnowIsNotSnowOnTheGround)
{
	// GetSnowAt / GetMaxRainingOrSnowingAt read the snow lying on the ground (not ported: 0), IsSnowingAt the
	// falling snow: a snow storm makes it snow, but neither cools a fire nor counts as snow for the trees
	magic::map_script::CreateWeatherClimate(0, 0, {2560.0f, 0.0f, 2560.0f}, 5120.0f, 5120.0f);
	storms::StormDescriptor d;
	d.position = {1000.0f, 0.0f, 1000.0f};
	d.innerRadius = 50.0f;
	d.outerRadius = 100.0f;
	d.fadeInTime = 1.0f;
	d.lifeTime = 1000.0f;
	d.weather = {};
	d.weather.snow = 90;
	d.weather.rain = 30;
	static_cast<void>(storms::Create(d));
	for (int i = 0; i < 20; ++i)
	{
		atmos::UpdateGame(12.0f, 0.1f);
	}
	const glm::vec3 at(1010.0f, 0.0f, 1030.0f);
	EXPECT_TRUE(IsSnowingAt(at));
	EXPECT_FLOAT_EQ(GetSnowAt(at), 0.0f);
	EXPECT_FLOAT_EQ(GetMaxRainingOrSnowingAt(at), GetRainAt(at));
}

TEST_F(WeatherTest, DeletionTakesTwoTurns)
{
	const auto id = MakeStorm({1000.0f, 0.0f, 1000.0f});
	storms::MarkForDeletion(id);
	EXPECT_EQ(storms::Find(id), nullptr);
	int count = 0;
	storms::ForEach([&count](const storms::Storm&) { ++count; });
	EXPECT_EQ(count, 1);
	atmos::UpdateGame(12.0f, 0.1f);
	count = 0;
	storms::ForEach([&count](const storms::Storm&) { ++count; });
	EXPECT_EQ(count, 1);
	atmos::UpdateGame(12.0f, 0.1f);
	count = 0;
	storms::ForEach([&count](const storms::Storm&) { ++count; });
	EXPECT_EQ(count, 0);
}

TEST_F(WeatherTest, KillStormsInArea)
{
	const auto id = MakeStorm({1000.0f, 0.0f, 1000.0f});
	storms::KillStormsInArea({1150.0f, 0.0f, 1000.0f}, 40.0f); // 150 >= 40 + 100: stays
	EXPECT_NE(storms::Find(id), nullptr);
	storms::KillStormsInArea({1150.0f, 0.0f, 1000.0f}, 60.0f); // 150 < 160
	EXPECT_EQ(storms::Find(id), nullptr);
}

TEST_F(WeatherTest, ClimatesAddTemperatureAndWind)
{
	// the order of Land 1's script: the local climates first, the world last (the list is newest first)
	magic::map_script::CreateWeatherClimate(1, 2, {2700.0f, 0.0f, 2560.0f}, 156.666672f, 321.666656f);
	magic::map_script::CreateWeatherClimateTemp(1, -35.2f, -32.0f);
	magic::map_script::CreateWeatherClimate(0, 0, {2560.0f, 0.0f, 2560.0f}, 5120.0f, 5120.0f);
	magic::map_script::CreateWeatherClimateTemp(0, 10.8f, 12.0f);
	magic::map_script::CreateWeatherClimateWind(0, 24.0f, 0.0f, 0.0f);

	// the world's temperature counts twice: trunc(10.8) + trunc(10 x 1 + 10)
	EXPECT_FLOAT_EQ(GetTemperatureAt({1000.0f, 0.0f, 1000.0f}), 20.0f);
	// and its wind: 24 + 24 = 48, 6 m/s
	EXPECT_FLOAT_EQ(GetWindAt({1000.0f, 0.0f, 1000.0f}).x, 6.0f);
	// inside the local climate's inner radius: + (-35)
	EXPECT_FLOAT_EQ(GetTemperatureAt({2700.0f, 0.0f, 2560.0f}), -15.0f);
	// half way between its radii: trunc(-35 x 0.5 + 20) = 2
	EXPECT_FLOAT_EQ(GetTemperatureAt({2700.0f + 239.16666f, 0.0f, 2560.0f}), 2.0f);
	// no storm: no rain
	EXPECT_FLOAT_EQ(GetMaxRainingOrSnowingAt({2700.0f, 0.0f, 2560.0f}), 0.0f);
	EXPECT_FALSE(IsRainingAt({2700.0f, 0.0f, 2560.0f}));
}

TEST_F(WeatherTest, TemperatureOscillatesAroundTheTarget)
{
	magic::map_script::CreateWeatherClimate(0, 0, {2560.0f, 0.0f, 2560.0f}, 5120.0f, 5120.0f);
	magic::map_script::CreateWeatherClimateTemp(0, 10.8f, 12.0f);
	auto& world = climate::World();
	// target = 12 (min = max): +1.2 while below, -1.2 once there, as the saved 10.8 / 12 of the land scripts
	climate::ProcessAll(0);
	EXPECT_NEAR(world.temperature, 12.0f, 1e-5f);
	climate::ProcessAll(1);
	EXPECT_NEAR(world.temperature, 10.8f, 1e-5f);
	// the first turn is a new day: the wind is min + max along the angle
	EXPECT_FLOAT_EQ(world.windX, 30.0f);
	EXPECT_FLOAT_EQ(world.windZ, 0.0f);
}

TEST_F(WeatherTest, RainDesireDrawsFromMinThenMax)
{
	// a new day: the rain desire grows by GameFloatRand(rainMin x 0.02) then GameFloatRand(rainMax x 0.02)
	const game_random::testing::ScopedState state;
	std::vector<float> draws;
	game_random::testing::SetGameRand(nullptr, [&draws](float x) {
		draws.push_back(x);
		return 0.0f;
	});
	magic::map_script::CreateWeatherClimate(0, 0, {2560.0f, 0.0f, 2560.0f}, 5120.0f, 5120.0f);
	climate::ProcessAll(0);
	ASSERT_GE(draws.size(), 2u);
	EXPECT_EQ(draws[0], static_cast<float>(static_cast<double>(0.1f) * 0.02));
	EXPECT_EQ(draws[1], static_cast<float>(static_cast<double>(0.3f) * 0.02));
}

TEST_F(WeatherTest, ClimateStormAtFullDesire)
{
	// The storm's size is GameRand(1000) clamped to 160..900: a draw of 0 gives the smallest storm
	const game_random::testing::ScopedState state;
	game_random::testing::SetGameRand([](uint32_t) { return 0u; }, nullptr);
	magic::map_script::CreateWeatherClimate(0, 0, {2560.0f, 0.0f, 2560.0f}, 5120.0f, 5120.0f);
	climate::Rain rain;
	rain.desire = 1.0f;
	climate::SetRain(0, rain);
	climate::ProcessAll(0); // a new day: desire 1 -> CreateStorm
	const auto& world = climate::World();
	ASSERT_EQ(world.storms.size(), 1u);
	EXPECT_EQ(world.rain.flags & 1, 1);
	EXPECT_FLOAT_EQ(world.rain.desire, 0.0f);
	const auto* storm = storms::Find(world.storms.front());
	ASSERT_NE(storm, nullptr);
	// the temperature went 12 -> 10.8 this turn: 10 degrees, snow int(exp(-(10 x 0.2)^2) x 100) = 1, rain 99;
	// overcast int(exp(-((10 - 30) / 15)^2) x 100) = 16; no raining days yet: life (rainMax x 100 - 0) x 10 = 300 s;
	// radius 160 (the draw of 0 above), outer int(160 x 1.1)
	EXPECT_EQ(storm->descriptor.weather.temperature, 10);
	EXPECT_EQ(storm->descriptor.weather.rain, 99);
	EXPECT_EQ(storm->descriptor.weather.snow, 1);
	EXPECT_EQ(storm->descriptor.weather.overcast, 16);
	EXPECT_EQ(storm->descriptor.weather.windX, 30);
	EXPECT_NEAR(storm->descriptor.lifeTime, 300.0f, 1e-3f);
	EXPECT_FLOAT_EQ(storm->descriptor.innerRadius, 160.0f);
	EXPECT_FLOAT_EQ(storm->descriptor.outerRadius, 176.0f);
	EXPECT_FLOAT_EQ(storm->descriptor.elevation, 500.0f);
}

TEST_F(WeatherTest, ScriptStormBytes)
{
	magic::map_script::CreateWeatherClimate(1, 1, {1000.0f, 0.0f, 1000.0f}, 100.0f, 200.0f);
	magic::map_script::CreateWeatherStorm(1, {1000.0f, 0.0f, 1000.0f}, 3.0f, 4, "10,20,1,50,1,1", "0.5,160,5,6,7,8",
	                                      "5,6,7,8,9,10", 2.0f, {1100.0f, 0.0f, 1000.0f});
	const auto* c = climate::Find(1);
	ASSERT_NE(c, nullptr);
	ASSERT_EQ(c->storms.size(), 1u);
	const auto* storm = storms::Find(c->storms.front());
	ASSERT_NE(storm, nullptr);
	EXPECT_FLOAT_EQ(storm->age, 3.0f);
	EXPECT_FLOAT_EQ(storm->speed, 2.0f);
	EXPECT_FLOAT_EQ(storm->descriptor.innerRadius, 10.0f);
	EXPECT_FLOAT_EQ(storm->descriptor.lifeTime, 50.0f);
	EXPECT_FLOAT_EQ(storm->descriptor.forkMin, 5.0f);
	EXPECT_FLOAT_EQ(storm->descriptor.sheetMax, 8.0f);
	EXPECT_EQ(storm->descriptor.numClouds, 4);
	// "%d" into bytes: overcast and snow are overwritten by the later writes
	EXPECT_EQ(storm->descriptor.weather.temperature, 7);
	EXPECT_EQ(storm->descriptor.weather.rain, 8);
	EXPECT_EQ(storm->descriptor.weather.snow, 0);
	EXPECT_EQ(storm->descriptor.weather.overcast, 0);
	EXPECT_EQ(storm->descriptor.weather.windX, 9);
	EXPECT_EQ(storm->descriptor.weather.windZ, 10);
}
