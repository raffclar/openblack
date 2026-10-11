/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <ECS/ScriptWeatherRules.h>
#include <gtest/gtest.h>

namespace
{
namespace weather = openblack::ecs::script_weather;
using openblack::ecs::components::Storm;
} // namespace

TEST(ScriptWeather, TheLostBrothersDrizzle)
{
	// The sick brother's storm: "degrees 15 rainfall 1 snowfall 0 overcast 1 fallspeed 2", "time 120 fade 40",
	// "clouds 8 shade 0.8 height 130", no lightning, "inner 600 outer 1000"
	Storm storm {};
	weather::SetWeather(storm, 15.0f, 1.0f, 0.0f, 1.0f, 2.0f);
	weather::SetTimeFade(storm, 120.0f, 40.0f);
	weather::SetClouds(storm, 8.0f, 0.8f, 130.0f);
	weather::SetLightning(storm, 0.0f, 0.0f, 0.0f, 0.0f);
	weather::SetInnerOuter(storm, 600.0f, 1000.0f);
	EXPECT_EQ(storm.effect.temperature, 15);
	EXPECT_EQ(storm.effect.rain, 100);
	EXPECT_EQ(storm.effect.snow, 0);
	EXPECT_EQ(storm.effect.overcast, 100);
	EXPECT_FLOAT_EQ(storm.rainSpeed, 2.0f);
	EXPECT_FLOAT_EQ(storm.lastsFor, 120.0f);
	EXPECT_FLOAT_EQ(storm.fadeTime, 40.0f);
	EXPECT_EQ(storm.cloudCount, 8);
	EXPECT_FLOAT_EQ(storm.cloudShade, 0.8f);
	EXPECT_FLOAT_EQ(storm.cloudHeight, 130.0f);
	EXPECT_EQ(storm.thunderWait.y, 0.0f);
	EXPECT_EQ(storm.boltWait.y, 0.0f);
	EXPECT_FLOAT_EQ(storm.innerRadius, 600.0f);
	EXPECT_FLOAT_EQ(storm.outerRadius, 1000.0f);
}

TEST(ScriptWeather, AmountsAreCutToWholePercentsAndCloudsToAWholeNumber)
{
	EXPECT_EQ(weather::Percent(0.999f), 99);
	EXPECT_EQ(weather::Percent(-0.5f), -50);
	// Past a byte the storm keeps only the low byte
	EXPECT_EQ(weather::Percent(2.0f), static_cast<int8_t>(200 - 256));
	Storm storm {};
	weather::SetClouds(storm, 3.9f, 0.5f, 100.0f);
	EXPECT_EQ(storm.cloudCount, 3);
}

TEST(ScriptWeather, SheetLightningThundersAndForksStrike)
{
	Storm storm {};
	weather::SetLightning(storm, 1.0f, 2.0f, 3.0f, 4.0f);
	EXPECT_EQ(storm.thunderWait, glm::vec2(1.0f, 2.0f));
	EXPECT_EQ(storm.boltWait, glm::vec2(3.0f, 4.0f));
}

TEST(ScriptWeather, ChangingAStormDrawsTheForkWaitThenTheFlashWait)
{
	Storm storm {};
	weather::SetLightning(storm, 1.0f, 2.0f, 3.0f, 7.0f);
	std::vector<float> asked;
	weather::DrawLightningWaits(storm, [&asked](float range) {
		asked.push_back(range);
		return range * 0.5f;
	});
	ASSERT_EQ(asked.size(), 2u);
	EXPECT_FLOAT_EQ(asked[0], 4.0f);
	EXPECT_FLOAT_EQ(asked[1], 1.0f);
	EXPECT_FLOAT_EQ(storm.boltTimer, 5.0f);
	EXPECT_FLOAT_EQ(storm.thunderTimer, 1.5f);
}
