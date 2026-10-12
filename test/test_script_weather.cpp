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

TEST(ScriptWeather, AClimateBlowsFullyWithinItsInnerRadiusAndFadesToItsOuter)
{
	const weather::ClimateWind climate {
	    .centre = {100.0f, 100.0f}, .innerRadius = 10.0f, .outerRadius = 30.0f, .windX = 20.0f, .windZ = 0.0f};
	EXPECT_FLOAT_EQ(*weather::ClimateShare(climate, {105.0f, 100.0f}), 1.0f);
	EXPECT_FLOAT_EQ(*weather::ClimateShare(climate, {120.0f, 100.0f}), 0.5f);
	EXPECT_FLOAT_EQ(*weather::ClimateShare(climate, {130.0f, 100.0f}), 0.0f);
	EXPECT_FALSE(weather::ClimateShare(climate, {131.0f, 100.0f}).has_value());
	// Inside the square round its reach but beyond its reach
	EXPECT_FALSE(weather::ClimateShare(climate, {125.0f, 125.0f}).has_value());
}

TEST(ScriptWeather, TheGlobalClimatesWindCountsTwiceOverTheIsland)
{
	// The global climate covers the whole map at full strength: its wind is added first and again among the climates
	const weather::ClimateWind global {
	    .centre = {2560.0f, 2560.0f}, .innerRadius = 5120.0f, .outerRadius = 5120.0f, .windX = 24.0f, .windZ = -3.5f};
	const std::vector<weather::ClimateWind> climates {global};
	const auto wind = weather::DriftWind({.x = 10, .z = 0}, global, climates, {1000.0f, 1000.0f});
	EXPECT_EQ(wind.x, 10 + 24 + 24);
	// Each part is cut towards nought
	EXPECT_EQ(wind.z, -3 + -3);
}

TEST(ScriptWeather, ALocalClimatesShareIsCutToAWholeNumberAsItIsAdded)
{
	const weather::ClimateWind global {
	    .centre = {0.0f, 0.0f}, .innerRadius = 0.0f, .outerRadius = 0.0f, .windX = 0.0f, .windZ = 0.0f};
	const std::vector<weather::ClimateWind> climates {
	    {.centre = {0.0f, 0.0f}, .innerRadius = 10.0f, .outerRadius = 30.0f, .windX = 21.0f, .windZ = 0.0f}};
	// Half its wind of 21 is 10.5, cut to 10
	const auto wind = weather::DriftWind({.x = 0, .z = 0}, global, climates, {20.0f, 0.0f});
	EXPECT_EQ(wind.x, 10);
	EXPECT_EQ(wind.z, 0);
}

TEST(ScriptWeather, TheWindsSumIsHeldWithinAByte)
{
	const weather::ClimateWind global {
	    .centre = {0.0f, 0.0f}, .innerRadius = 0.0f, .outerRadius = 0.0f, .windX = 100.0f, .windZ = -100.0f};
	const auto wind = weather::DriftWind({.x = 100, .z = -100}, global, {}, {500.0f, 500.0f});
	EXPECT_EQ(wind.x, 127);
	EXPECT_EQ(wind.z, -128);
}

TEST(ScriptWeather, AStormTheWindBlowsMovesAHundredthOfItsWindATurn)
{
	const auto moved = weather::Drift({100.0f, 5.0f, 200.0f}, {.x = 50, .z = -20});
	EXPECT_FLOAT_EQ(moved.x, 100.5f);
	EXPECT_FLOAT_EQ(moved.y, 5.0f);
	EXPECT_FLOAT_EQ(moved.z, 199.8f);
}

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

TEST(ScriptWeather, ALandStormReadsItsShapeCloudsAndWeather)
{
	// The first storm of the Three Gods playground
	const auto storm =
	    weather::ParseLandStorm("160.00,176.00,10.00,460.00,1.00,1.10", "0.12,500.00,0.00,0.00,0.00,0.00", "0,0,8,93,-3,39");
	EXPECT_FLOAT_EQ(storm.innerRadius, 160.0f);
	EXPECT_FLOAT_EQ(storm.outerRadius, 176.0f);
	EXPECT_FLOAT_EQ(storm.fadeSeconds, 10.0f);
	EXPECT_FLOAT_EQ(storm.lastsFor, 460.0f);
	EXPECT_FLOAT_EQ(storm.strength, 1.0f);
	EXPECT_FLOAT_EQ(storm.rainSpeed, 1.1f);
	EXPECT_FLOAT_EQ(storm.cloudShade, 0.12f);
	EXPECT_FLOAT_EQ(storm.cloudHeight, 500.0f);
	EXPECT_EQ(storm.boltWait, glm::vec2(0.0f));
	EXPECT_EQ(storm.thunderWait, glm::vec2(0.0f));
	EXPECT_EQ(storm.effect.temperature, 8);
	EXPECT_EQ(storm.effect.rain, 93);
	EXPECT_EQ(storm.effect.snow, 0);
	EXPECT_EQ(storm.effect.overcast, 0);
	EXPECT_EQ(storm.effect.windX, -3);
	EXPECT_EQ(storm.effect.windZ, 39);
	EXPECT_EQ(storm.effect.snowCover, 0);
}

TEST(ScriptWeather, ALandStormsLightningWaitsAreForksThenFlashes)
{
	const auto storm = weather::ParseLandStorm("", "0.5,160,1,2,3,4", "");
	EXPECT_EQ(storm.boltWait, glm::vec2(1.0f, 2.0f));
	EXPECT_EQ(storm.thunderWait, glm::vec2(3.0f, 4.0f));
}

TEST(ScriptWeather, ALandStormsWeatherNumbersSpillIntoTheBytesAfterThem)
{
	// The cloud and snow are overwritten by the temperature and rain stored after them; the wind along spills on
	const auto storm = weather::ParseLandStorm("", "", "50,20,8,-1,-3,-39");
	EXPECT_EQ(storm.effect.temperature, 8);
	EXPECT_EQ(storm.effect.rain, -1);
	EXPECT_EQ(storm.effect.snow, -1);
	EXPECT_EQ(storm.effect.overcast, -1);
	EXPECT_EQ(storm.effect.windX, -3);
	EXPECT_EQ(storm.effect.windZ, -39);
	EXPECT_EQ(storm.effect.snowCover, -1);
	EXPECT_EQ(storm.effect.stamp, 0xff);
}

TEST(ScriptWeather, ALandStormKeepsWhatItsListsLeaveOut)
{
	const auto storm = weather::ParseLandStorm("300, 400", "0.25 ,90", "5,x");
	EXPECT_FLOAT_EQ(storm.innerRadius, 300.0f);
	EXPECT_FLOAT_EQ(storm.outerRadius, 400.0f);
	EXPECT_FLOAT_EQ(storm.fadeSeconds, 10.0f);
	EXPECT_FLOAT_EQ(storm.lastsFor, 100.0f);
	EXPECT_FLOAT_EQ(storm.strength, 1.0f);
	EXPECT_FLOAT_EQ(storm.rainSpeed, 1.0f);
	// Reading stops where a number isn't followed straight by a comma
	EXPECT_FLOAT_EQ(storm.cloudShade, 0.25f);
	EXPECT_FLOAT_EQ(storm.cloudHeight, 160.0f);
	// Only the cloud is read, and its upper bytes clear the wind across, the wind along and the snow cover
	EXPECT_EQ(storm.effect.temperature, 10);
	EXPECT_EQ(storm.effect.rain, 100);
	EXPECT_EQ(storm.effect.snow, 0);
	EXPECT_EQ(storm.effect.overcast, 5);
	EXPECT_EQ(storm.effect.windX, 0);
	EXPECT_EQ(storm.effect.windZ, 0);
}
