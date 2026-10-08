/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Debug/WeatherModel.h"

using namespace openblack;
using namespace openblack::debug::weather_window;

TEST(WeatherMenu, WindBlowsFromItsDirection)
{
	// From the north (0 degrees) all of it is on z, from the east (90) all on x
	EXPECT_EQ(WindBytes(0.0f, 30), glm::i8vec2(0, 30));
	EXPECT_EQ(WindBytes(90.0f, 30), glm::i8vec2(30, 0));
	EXPECT_EQ(WindBytes(180.0f, 30), glm::i8vec2(0, -30));
	EXPECT_EQ(WindBytes(45.0f, 100), glm::i8vec2(71, 71));
	EXPECT_EQ(WindBytes(123.0f, 0), glm::i8vec2(0, 0));
}

TEST(WeatherMenu, WindStaysWithinAByte)
{
	EXPECT_EQ(WindBytes(90.0f, 500), glm::i8vec2(127, 0));
	EXPECT_EQ(WindBytes(270.0f, 500), glm::i8vec2(-128, 0));
}

TEST(WeatherMenu, PresetKeepsDirectionAndLength)
{
	StormSettings settings;
	settings.windDegrees = 200.0f;
	settings.seconds = 42.0f;
	const auto& thunderstorm = k_Presets[3];
	ApplyPreset(thunderstorm, settings);
	EXPECT_EQ(settings.rain, 100);
	EXPECT_EQ(settings.overcast, 100);
	EXPECT_EQ(settings.temperature, 20);
	EXPECT_EQ(settings.windStrength, 30);
	EXPECT_TRUE(settings.lightning);
	EXPECT_EQ(settings.thunderWait, k_ThunderWait);
	EXPECT_EQ(settings.boltWait, k_BoltWait);
	EXPECT_EQ(settings.windDegrees, 200.0f);
	EXPECT_EQ(settings.seconds, 42.0f);

	ApplyPreset(k_Presets[2], settings);
	EXPECT_FALSE(settings.lightning);
	EXPECT_EQ(settings.thunderWait, glm::vec2(0.0f));
	EXPECT_EQ(settings.boltWait, glm::vec2(0.0f));
}

TEST(WeatherMenu, OnlyClearForcesNoStorm)
{
	EXPECT_FALSE(ForcesStorm(k_Presets[0]));
	for (size_t i = 1; i < k_Presets.size(); ++i)
	{
		EXPECT_TRUE(ForcesStorm(k_Presets[i])) << k_Presets[i].name;
	}
}

TEST(WeatherMenu, LightningSwitch)
{
	StormSettings settings;
	SetLightning(settings, true);
	EXPECT_EQ(settings.thunderWait, k_ThunderWait);
	EXPECT_EQ(settings.boltWait, k_BoltWait);
	// Intervals already set are kept
	settings.thunderWait = {1.0f, 2.0f};
	SetLightning(settings, true);
	EXPECT_EQ(settings.thunderWait, glm::vec2(1.0f, 2.0f));
	SetLightning(settings, false);
	EXPECT_FALSE(settings.lightning);
	EXPECT_EQ(settings.thunderWait, glm::vec2(0.0f));
	EXPECT_EQ(settings.boltWait, glm::vec2(0.0f));
}

TEST(WeatherMenu, IslandStormCoversTheIsland)
{
	StormSettings settings;
	settings.rain = 30;
	settings.overcast = 60;
	settings.temperature = -5;
	settings.windDegrees = 90.0f;
	settings.windStrength = 10;
	settings.seconds = 120.0f;
	settings.fadeSeconds = 4.0f;
	settings.cloudHeight = 200.0f;
	settings.rainSpeed = 2.0f;
	SetLightning(settings, true);
	const auto storm = IslandStorm(settings);
	EXPECT_EQ(storm.position, glm::vec3(2560.0f, 0.0f, 2560.0f));
	// The corners of the 5120 m island are inside the full-strength radius
	EXPECT_GT(storm.innerRadius * storm.innerRadius, 2.0f * 2560.0f * 2560.0f);
	EXPECT_GT(storm.outerRadius, storm.innerRadius);
	EXPECT_EQ(storm.lifeTime, 120.0f);
	EXPECT_EQ(storm.fadeInTime, 4.0f);
	EXPECT_EQ(storm.strength, 1.0f);
	EXPECT_EQ(storm.numClouds, 0);
	EXPECT_EQ(storm.elevation, 200.0f);
	EXPECT_EQ(storm.fallSpeed, 2.0f);
	EXPECT_EQ(storm.sheetMin, k_ThunderWait.x);
	EXPECT_EQ(storm.sheetMax, k_ThunderWait.y);
	EXPECT_EQ(storm.forkMin, k_BoltWait.x);
	EXPECT_EQ(storm.forkMax, k_BoltWait.y);
	EXPECT_EQ(storm.weather.temperature, -5);
	EXPECT_EQ(storm.weather.rain, 30);
	EXPECT_EQ(storm.weather.snow, 0);
	EXPECT_EQ(storm.weather.overcast, 60);
	EXPECT_EQ(storm.weather.windX, 10);
	EXPECT_EQ(storm.weather.windZ, 0);
	EXPECT_TRUE(HasLightning(storm));

	SetLightning(settings, false);
	EXPECT_FALSE(HasLightning(IslandStorm(settings)));
}

TEST(WeatherMenu, StormPhases)
{
	weather::storms::Storm storm;
	storm.descriptor.fadeInTime = 10.0f;
	storm.descriptor.lifeTime = 100.0f;
	storm.age = 5.0f;
	EXPECT_EQ(PhaseOf(storm), StormPhase::Live);
	storm.age = 50.0f;
	EXPECT_EQ(PhaseOf(storm), StormPhase::Live);
	storm.age = 95.0f;
	EXPECT_EQ(PhaseOf(storm), StormPhase::Clearing);
	storm.deleteCounter = 1;
	EXPECT_EQ(PhaseOf(storm), StormPhase::Ending);
}

TEST(WeatherMenu, Percentages)
{
	EXPECT_EQ(LyingPercent(127), 100);
	EXPECT_EQ(LyingPercent(0), 0);
	EXPECT_EQ(LyingPercent(-20), 0);
	EXPECT_EQ(FlashPercent(255), 100);
	EXPECT_EQ(FlashPercent(128), 50);
}
