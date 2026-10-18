/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the volumetric clouds

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <numbers>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Graphics/VolumetricClouds.h"

namespace clouds = openblack::graphics::volumetric_clouds;

TEST(VolumetricClouds, TheNoiseTiles)
{
	// A point and the same point one tile on give the same value
	const glm::vec3 p {0.13f, 0.71f, 0.42f};
	EXPECT_NEAR(clouds::Worley(p, 4, 3), clouds::Worley(p + glm::vec3(1.0f, 0.0f, 0.0f), 4, 3), 1e-5f);
	EXPECT_NEAR(clouds::Worley(p, 4, 3), clouds::Worley(p + glm::vec3(0.0f, 1.0f, 1.0f), 4, 3), 1e-5f);
	// The gradient noise repeats after its period of cells
	const glm::vec3 q {1.3f, 2.7f, 0.4f};
	EXPECT_NEAR(clouds::Perlin(q, 4, 9), clouds::Perlin(q + glm::vec3(4.0f, 0.0f, 0.0f), 4, 9), 1e-5f);
	EXPECT_NEAR(clouds::Perlin(q, 4, 9), clouds::Perlin(q + glm::vec3(0.0f, 4.0f, -4.0f), 4, 9), 1e-5f);
	// The gradient noise is 0 on its lattice
	EXPECT_NEAR(clouds::Perlin(glm::vec3(2.0f, 3.0f, 1.0f), 4, 9), 0.0f, 1e-6f);
}

TEST(VolumetricClouds, TheCellularNoiseIsOneAtItsPointsAndFallsAway)
{
	// Somewhere in each cell the noise is near 1, and its values stay from 0 to 1
	float highest = 0.0f;
	float lowest = 1.0f;
	for (int i = 0; i < 4000; ++i)
	{
		const glm::vec3 p {static_cast<float>(i % 20) / 20.0f, static_cast<float>((i / 20) % 20) / 20.0f,
		                   static_cast<float>(i / 400) / 10.0f};
		const float value = clouds::Worley(p, 2, 5);
		highest = std::max(highest, value);
		lowest = std::min(lowest, value);
	}
	EXPECT_GT(highest, 0.85f);
	EXPECT_LT(lowest, 0.45f);
	EXPECT_GE(lowest, 0.0f);
	EXPECT_LE(highest, 1.0f);
}

TEST(VolumetricClouds, TheVolumesHaveTheirSizes)
{
	const auto base = clouds::BaseNoiseVolume(16);
	EXPECT_EQ(base.size(), 16u * 16u * 16u * 4u);
	const auto detail = clouds::DetailNoiseVolume(8);
	EXPECT_EQ(detail.size(), 8u * 8u * 8u * 4u);
	// The same noise each time: nothing depends on the time or on the threads
	EXPECT_EQ(base, clouds::BaseNoiseVolume(16));
	// The noise is not flat
	const auto [low, high] = std::ranges::minmax_element(base);
	EXPECT_GT(*high - *low, 100);
}

TEST(VolumetricClouds, TheWeatherNoiseGivesTheCoverThatIsAskedFor)
{
	constexpr uint32_t k_Side = 64;
	const auto weather = clouds::WeatherNoise(k_Side);
	ASSERT_EQ(weather.size(), k_Side * k_Side * 4u);
	// The cover channel spreads evenly: a threshold at 1 - c keeps about a share c of the texels
	for (const float cover : {0.2f, 0.5f, 0.8f})
	{
		size_t kept = 0;
		for (size_t i = 0; i < k_Side * k_Side; ++i)
		{
			kept += static_cast<float>(weather[i * 4]) / 255.0f > 1.0f - cover ? 1 : 0;
		}
		EXPECT_NEAR(static_cast<float>(kept) / static_cast<float>(k_Side * k_Side), cover, 0.02f);
	}
}

TEST(VolumetricClouds, TheBlueNoiseHasEachValueAsOftenAndNeighboursDiffer)
{
	constexpr uint32_t k_Side = 32;
	const auto noise = clouds::BlueNoise(k_Side);
	ASSERT_EQ(noise.size(), k_Side * k_Side);
	std::array<int, 256> counts {};
	for (const auto value : noise)
	{
		++counts.at(value);
	}
	EXPECT_TRUE(std::ranges::all_of(counts, [](int count) { return count == 4; }));
	// White noise has a mean difference of about 85 between neighbours; blue noise has more
	double difference = 0.0;
	for (uint32_t y = 0; y < k_Side; ++y)
	{
		for (uint32_t x = 0; x < k_Side; ++x)
		{
			const int a = noise[y * k_Side + x];
			const int b = noise[y * k_Side + (x + 1) % k_Side];
			difference += std::abs(a - b);
		}
	}
	EXPECT_GT(difference / (k_Side * k_Side), 95.0);
}

TEST(VolumetricClouds, EachTypeHasItsShapeOfHeight)
{
	// No cloud under the bottom or over the top of the type
	for (const float type : {clouds::k_Stratus, clouds::k_Cumulus, clouds::k_Cumulonimbus})
	{
		EXPECT_EQ(clouds::HeightProfile(type, -0.1f), 0.0f);
		EXPECT_EQ(clouds::HeightProfile(type, 1.1f), 0.0f);
	}
	// A stratus is a thin sheet: little at two thirds of its height; a cumulonimbus is still full there
	EXPECT_LT(clouds::HeightProfile(clouds::k_Stratus, 0.7f), 0.4f);
	EXPECT_GT(clouds::HeightProfile(clouds::k_Cumulonimbus, 0.7f), 0.95f);
	EXPECT_GT(clouds::HeightProfile(clouds::k_Cumulus, 0.3f), 0.95f);
	// The towers are taller
	EXPECT_LT(clouds::LayerDepth(clouds::k_Stratus), clouds::LayerDepth(clouds::k_Cumulus));
	EXPECT_LT(clouds::LayerDepth(clouds::k_Cumulus), clouds::LayerDepth(clouds::k_Cumulonimbus));
}

TEST(VolumetricClouds, ThePhaseScattersForwardAndKeepsTheEnergy)
{
	// The integral over the sphere is 1 for each lobe and for the two together
	for (const float g : {0.0f, 0.8f, -0.35f})
	{
		double sum = 0.0;
		constexpr int k_Steps = 20000;
		for (int i = 0; i < k_Steps; ++i)
		{
			const double theta = (i + 0.5) / k_Steps * std::numbers::pi;
			sum += clouds::HenyeyGreenstein(static_cast<float>(std::cos(theta)), g) * 2.0 * std::numbers::pi * std::sin(theta) *
			       (std::numbers::pi / k_Steps);
		}
		EXPECT_NEAR(sum, 1.0, 1e-3);
	}
	// More light towards the sun than away from it, and a little more back than to the side
	EXPECT_GT(clouds::DualLobePhase(1.0f), clouds::DualLobePhase(-1.0f));
	EXPECT_GT(clouds::DualLobePhase(-1.0f), clouds::DualLobePhase(0.0f));
}

TEST(VolumetricClouds, TheLightFallsThroughTheCloud)
{
	// Beer-Lambert: less light through more cloud
	EXPECT_NEAR(clouds::BeerPowder(0.0f, 10.0f, 1.0f), 1.0f, 1e-6f);
	EXPECT_GT(clouds::BeerPowder(1.0f, 10.0f, 1.0f), clouds::BeerPowder(2.0f, 10.0f, 1.0f));
	// The powder term: a thin edge seen from the side of the light is darker than seen against the light
	EXPECT_LT(clouds::BeerPowder(0.5f, 0.05f, -1.0f), clouds::BeerPowder(0.5f, 0.05f, 1.0f));
	// The light that scatters many times goes deeper than the light that scatters once
	const float once = std::exp(-8.0f) * clouds::DualLobePhase(0.0f);
	EXPECT_GT(clouds::MultipleScattering(8.0f, 0.0f), once * 10.0f);
	EXPECT_GT(clouds::MultipleScattering(1.0f, 0.0f), clouds::MultipleScattering(4.0f, 0.0f));
}

TEST(VolumetricClouds, TheWeatherEasesWithNoJump)
{
	clouds::CloudClimate climate;
	clouds::CloudWeather calm;
	calm.cover = 0.3f;
	climate.Reset(calm);
	clouds::CloudWeather storm = calm;
	storm.cover = 1.0f;
	float before = climate.Current().cover;
	float largestStep = 0.0f;
	// Frames at 100 a second for two minutes: the cover rises slowly at first, and never by much in one frame
	for (int frame = 0; frame < 12000; ++frame)
	{
		climate.Step(0.01f, storm, {});
		const float now = climate.Current().cover;
		EXPECT_GE(now, before - 1e-6f);
		largestStep = std::max(largestStep, now - before);
		before = now;
		if (frame == 99)
		{
			// After one second, little has changed
			EXPECT_LT(now, 0.33f);
		}
	}
	EXPECT_LT(largestStep, 0.002f);
	EXPECT_NEAR(climate.Current().cover, 1.0f, 0.01f);
	// It calms down more slowly than it built up
	clouds::CloudClimate up;
	up.Reset(calm);
	clouds::CloudClimate down;
	down.Reset(storm);
	for (int frame = 0; frame < 2000; ++frame)
	{
		up.Step(0.01f, storm, {});
		down.Step(0.01f, calm, {});
	}
	EXPECT_GT(up.Current().cover - 0.3f, 1.0f - down.Current().cover);
}

TEST(VolumetricClouds, TheDriftAddsTheWindsTravelEachFrame)
{
	clouds::CloudClimate climate;
	clouds::CloudWeather weather;
	weather.wind = {10.0f, -5.0f};
	climate.Reset(weather);
	for (int frame = 0; frame < 300; ++frame)
	{
		climate.Step(1.0f / 60.0f, weather, {});
	}
	EXPECT_NEAR(climate.Drift().x, 50.0f, 0.01f);
	EXPECT_NEAR(climate.Drift().y, -25.0f, 0.01f);
	// A change of the wind turns the drift slowly: the drift does not jump to the new speed times the time
	weather.wind = {-10.0f, 0.0f};
	climate.Step(1.0f / 60.0f, weather, {});
	EXPECT_GT(climate.Drift().x, 50.0f);
	// The shapes change all the time
	EXPECT_GT(climate.Evolution(), 0.0f);
}

TEST(VolumetricClouds, AStormsCellBuildsAndCalmsAfterItsStormGoes)
{
	clouds::CloudClimate climate;
	climate.Reset({});
	const std::array storms {clouds::StormInput {
	    .serial = 7, .centre = {100.0f, 200.0f}, .innerRadius = 300.0f, .outerRadius = 900.0f, .strength = 1.0f, .rain = 1.0f}};
	climate.Step(0.01f, {}, storms);
	ASSERT_EQ(climate.Storms().size(), 1u);
	EXPECT_LT(clouds::CloudClimate::Strength(climate.Storms()[0]), 0.01f);
	for (int frame = 0; frame < 12000; ++frame)
	{
		climate.Step(0.01f, {}, storms);
	}
	EXPECT_NEAR(clouds::CloudClimate::Strength(climate.Storms()[0]), 1.0f, 0.01f);
	// The storm goes: its cell stays and calms down, then goes too
	climate.Step(0.01f, {}, {});
	ASSERT_EQ(climate.Storms().size(), 1u);
	EXPECT_GT(clouds::CloudClimate::Strength(climate.Storms()[0]), 0.9f);
	for (int frame = 0; frame < 30000 && !climate.Storms().empty(); ++frame)
	{
		climate.Step(0.01f, {}, {});
	}
	EXPECT_TRUE(climate.Storms().empty());
}

TEST(VolumetricClouds, ANewChoiceOfCoverShowsAtOnce)
{
	clouds::CloudClimate climate;
	clouds::CloudWeather weather;
	weather.cover = 0.6f;
	climate.Reset(weather);
	const std::array storms {clouds::StormInput {
	    .serial = 3, .centre = {0.0f, 0.0f}, .innerRadius = 300.0f, .outerRadius = 900.0f, .strength = 1.0f, .rain = 0.5f}};
	for (int frame = 0; frame < 12000; ++frame)
	{
		climate.Step(0.01f, weather, storms);
	}
	ASSERT_EQ(climate.Storms().size(), 1u);
	// The clear choice puts its cover at once, and the storm that has gone takes its cell with it
	const auto clear = clouds::ChosenWeather(clouds::CoverChoice::Clear, weather);
	EXPECT_EQ(clear.choice, clouds::CoverChoice::Clear);
	climate.Step(0.01f, clear, {});
	EXPECT_FLOAT_EQ(climate.Current().cover, clear.cover);
	EXPECT_EQ(climate.Current().choice, clouds::CoverChoice::Clear);
	EXPECT_TRUE(climate.Storms().empty());
	// Back to the weather's own cover: it is put at once too, with the storm's cell at its strength
	const auto own = clouds::ChosenWeather(clouds::CoverChoice::Weather, weather);
	climate.Step(0.01f, own, storms);
	EXPECT_FLOAT_EQ(climate.Current().cover, weather.cover);
	ASSERT_EQ(climate.Storms().size(), 1u);
	EXPECT_FLOAT_EQ(clouds::CloudClimate::Strength(climate.Storms()[0]), 1.0f);
	// A change of the weather with the same choice eases as before
	auto heavier = own;
	heavier.cover = 0.9f;
	climate.Step(0.01f, heavier, storms);
	EXPECT_LT(climate.Current().cover, 0.61f);
}

TEST(VolumetricClouds, TheWeatherPicksTheTypeOfCloud)
{
	const auto fair = clouds::WeatherFor(0.0f, 0.0f, clouds::CloudWind({1.0f, 0.0f}, 3.0f));
	EXPECT_NEAR(fair.cover, clouds::k_FairCover, 1e-6f);
	EXPECT_NEAR(fair.type, clouds::k_Cumulus, 1e-6f);
	// A calm day's wind moves the clouds as fast as the original game's clouds, and a strong wind faster
	EXPECT_NEAR(glm::length(fair.wind), clouds::k_CalmWind, 1e-3f);
	EXPECT_GT(glm::length(clouds::CloudWind({0.0f, 1.0f}, 15.0f)), 2.0f * clouds::k_CalmWind);
	// Overcast with no rain gives a sheet; rain gives towers
	EXPECT_LT(clouds::WeatherFor(1.0f, 0.0f, {}).type, 0.1f);
	EXPECT_GT(clouds::WeatherFor(1.0f, 1.0f, {}).type, 0.95f);
	EXPECT_GT(clouds::WeatherFor(1.0f, 1.0f, {}).cover, 0.9f);
	// A debug choice puts its own cover in place of the weather's
	EXPECT_LT(clouds::ChosenWeather(clouds::CoverChoice::Clear, fair).cover, 0.2f);
	EXPECT_EQ(clouds::ChosenWeather(clouds::CoverChoice::Weather, fair).cover, fair.cover);
}

TEST(VolumetricClouds, FromHighUpTheCloudsAndTheirShadowReachFarther)
{
	EXPECT_FLOAT_EQ(clouds::ShadowWindowFor(100.0f), clouds::k_ShadowWindow);
	EXPECT_FLOAT_EQ(clouds::ShadowWindowFor(clouds::k_ShadowWindowHeight), clouds::k_ShadowWindow);
	EXPECT_FLOAT_EQ(clouds::ShadowWindowFor(clouds::k_ShadowWindowHeight * 3.0f), clouds::k_ShadowWindow * 4.0f);
	EXPECT_GE(clouds::ShadowWindowFor(8000.0f), 8000.0f * 2.0f);
	EXPECT_FLOAT_EQ(clouds::FarthestCloudFor(100.0f), clouds::k_FarthestCloud);
	EXPECT_GT(clouds::FarthestCloudFor(10000.0f), clouds::FarthestCloudFor(5000.0f));
	EXPECT_NEAR(clouds::FarthestCloudFor(60900.0f), clouds::k_FarthestCloudCap, 1000.0f);
	// From space the clouds reach past the horizon of the world's ball
	EXPECT_GT(clouds::FarthestCloudFor(1e6f), 3.0e6f);
	// A texel of a wide map lies on whole texels too
	const float window = clouds::ShadowWindowFor(9000.0f);
	const auto origin = clouds::ShadowOrigin({1234.5f, 2345.6f}, window);
	EXPECT_NEAR(std::fmod(origin.x, window / static_cast<float>(clouds::k_ShadowTexels)), 0.0f, 1e-2f);
}

TEST(VolumetricClouds, TheShadowLiesOnWholeTexelsAndThePacksKeepTheStrongStorms)
{
	constexpr float k_Texel = clouds::k_ShadowWindow / static_cast<float>(clouds::k_ShadowTexels);
	const auto origin = clouds::ShadowOrigin({1234.5f, 2345.6f}, clouds::k_ShadowWindow);
	EXPECT_NEAR(std::fmod(origin.x, k_Texel), 0.0f, 1e-3f);
	EXPECT_LE(std::abs(origin.x + clouds::k_ShadowWindow * 0.5f - 1234.5f), k_Texel);

	std::vector<clouds::StormCell> cells(2);
	cells[0].centre = {1.0f, 2.0f};
	cells[0].innerRadius = 10.0f;
	cells[0].outerRadius = 20.0f;
	cells[0].strength = {0.5f, 0.5f};
	cells[1].strength = {0.0f, 0.0f};
	clouds::CloudUniforms uniforms;
	clouds::PackStorms(cells, uniforms);
	EXPECT_EQ(uniforms.cover.w, 1.0f);
	EXPECT_EQ(uniforms.storms[0], glm::vec4(1.0f, 2.0f, 10.0f, 20.0f));
	EXPECT_EQ(uniforms.storms[clouds::k_MaxStorms].x, 0.5f);
}

TEST(VolumetricClouds, TheLowSunGoesRed)
{
	// Straight above, all the light comes through; near the horizon the blue goes first, then the green
	const auto noon = clouds::SunThroughAir(1.0f);
	EXPECT_NEAR(noon.r, 1.0f, 1e-4f);
	EXPECT_NEAR(noon.b, 1.0f, 1e-4f);
	const auto evening = clouds::SunThroughAir(std::sin(0.08f));
	EXPECT_GT(evening.r, evening.g);
	EXPECT_GT(evening.g, evening.b);
	EXPECT_LT(evening.b, 0.4f);
	EXPECT_GT(evening.r, 0.5f);
	// Lower still, less of each
	const auto sunset = clouds::SunThroughAir(0.0f);
	EXPECT_LT(sunset.r, evening.r);
	EXPECT_LT(sunset.b, 0.05f);
}
