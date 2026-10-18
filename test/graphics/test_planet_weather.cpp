/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Graphics/PlanetWeather.h"

namespace planet = openblack::graphics::planet_weather;

namespace
{

// Directions spread over the whole sphere (a Fibonacci lattice)
std::vector<glm::vec3> SphereDirections(uint32_t count)
{
	std::vector<glm::vec3> directions;
	const float golden = 2.39996323f;
	for (uint32_t i = 0; i < count; ++i)
	{
		const float y = 1.0f - 2.0f * (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
		const float r = std::sqrt(1.0f - y * y);
		const float a = golden * static_cast<float>(i);
		directions.emplace_back(r * std::cos(a), y, r * std::sin(a));
	}
	return directions;
}

} // namespace

TEST(PlanetWeather, IslandIsAtItsLatitude)
{
	EXPECT_NEAR(planet::LatitudeOf({0.0f, 1.0f, 0.0f}), planet::k_IslandLatitude, 1e-3f);
	EXPECT_NEAR(planet::LatitudeOf(planet::NorthPole()), 90.0f, 1e-3f);
	// The map's middle is straight over the planet's middle
	EXPECT_NEAR(glm::distance(planet::DirectionAt({0.0f, 0.0f}, 6.0e6f), glm::vec3(0.0f, 1.0f, 0.0f)), 0.0f, 1e-6f);
}

TEST(PlanetWeather, LatitudeBandsAreCloudyAtTheEquatorAndClearInTheSubtropics)
{
	EXPECT_GT(planet::LatitudeBands(0.0f), 0.1f);
	EXPECT_LT(planet::LatitudeBands(24.0f), -0.15f);
	EXPECT_LT(planet::LatitudeBands(-24.0f), -0.15f);
	EXPECT_GT(planet::LatitudeBands(52.0f), 0.1f);
}

TEST(PlanetWeather, CubeFacesLookAlongTheirAxes)
{
	const std::array<glm::vec3, 6> axes {glm::vec3 {1, 0, 0},  glm::vec3 {-1, 0, 0}, glm::vec3 {0, 1, 0},
	                                     glm::vec3 {0, -1, 0}, glm::vec3 {0, 0, 1},  glm::vec3 {0, 0, -1}};
	for (uint32_t face = 0; face < 6; ++face)
	{
		EXPECT_NEAR(glm::distance(planet::CubeDirection(face, {0.5f, 0.5f}), axes.at(face)), 0.0f, 1e-6f);
		// The corners of the faces meet: each corner direction is a diagonal of the cube
		const auto corner = planet::CubeDirection(face, {0.0f, 0.0f});
		EXPECT_NEAR(std::abs(corner.x), std::abs(corner.y), 1e-6f);
		EXPECT_NEAR(std::abs(corner.y), std::abs(corner.z), 1e-6f);
	}
}

TEST(PlanetWeather, MapHasClearAreasCloudyAreasAndFronts)
{
	int clear = 0;
	int cloudy = 0;
	int fronts = 0;
	float sum = 0.0f;
	const auto directions = SphereDirections(3000);
	for (const auto& d : directions)
	{
		const auto weather = planet::WeatherAt(d);
		ASSERT_GE(weather.cover, 0.0f);
		ASSERT_LE(weather.cover, 1.0f);
		ASSERT_GE(weather.type, 0.0f);
		ASSERT_LE(weather.type, 1.0f);
		clear += weather.cover < 0.15f ? 1 : 0;
		cloudy += weather.cover > 0.85f ? 1 : 0;
		fronts += weather.front > 0.5f ? 1 : 0;
		sum += weather.cover;
	}
	EXPECT_GT(clear, 100);
	EXPECT_GT(cloudy, 100);
	EXPECT_GT(fronts, 30);
	const float mean = sum / static_cast<float>(directions.size());
	EXPECT_GT(mean, 0.35f);
	EXPECT_LT(mean, 0.65f);
}

TEST(PlanetWeather, MapDoesNotRepeatAlongAGreatCircle)
{
	// Along a great circle the cover never comes back to the same values a step later, as a tiled map would
	std::vector<float> cover;
	for (int i = 0; i < 720; ++i)
	{
		const float a = static_cast<float>(i) * 0.00872665f;
		cover.push_back(planet::WeatherAt({std::cos(a), 0.3f, std::sin(a)}).cover);
	}
	for (size_t shift = 20; shift < 360; ++shift)
	{
		float difference = 0.0f;
		for (size_t i = 0; i + shift < cover.size(); ++i)
		{
			difference += std::abs(cover[i] - cover[i + shift]);
		}
		EXPECT_GT(difference / static_cast<float>(cover.size() - shift), 0.03f) << shift;
	}
}

TEST(PlanetWeather, CubeHoldsTheMap)
{
	constexpr uint32_t k_Side = 8;
	const auto texels = planet::CubeTexels(k_Side);
	ASSERT_EQ(texels.size(), static_cast<size_t>(k_Side) * k_Side * 4 * 6);
	const auto weather = planet::WeatherAt(planet::CubeDirection(2, {0.5f / k_Side, 0.5f / k_Side}));
	EXPECT_NEAR(static_cast<float>(texels.at(2 * k_Side * k_Side * 4)) / 255.0f, weather.cover, 1.0f / 255.0f);
}

TEST(PlanetWeather, CoverChangesFromRegionToRegionNearTheIsland)
{
	// The cover along a line through the island, every 20 km over 3000 km
	constexpr float k_Radius = 6371.0f * 949.0f;
	constexpr float k_Step = 20.0f * 949.0f;
	std::vector<float> cover;
	for (int i = -75; i <= 75; ++i)
	{
		cover.push_back(planet::WeatherAt(planet::DirectionAt({static_cast<float>(i) * k_Step, 0.0f}, k_Radius)).cover);
	}
	// From one place to the place 100 km away the cover changes much: there is no even cover over the regions
	float difference = 0.0f;
	constexpr size_t k_Lag = 5;
	for (size_t i = 0; i + k_Lag < cover.size(); ++i)
	{
		difference += std::abs(cover.at(i + k_Lag) - cover.at(i));
	}
	EXPECT_GT(difference / static_cast<float>(cover.size() - k_Lag), 0.2f);
}
