/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the moon that looks like the real moon

#include <cmath>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Graphics/Atmosphere.h"
#include "Graphics/RealMoon.h"

namespace moon = openblack::graphics::real_moon;
namespace atmos = openblack::graphics::atmosphere;

namespace
{
constexpr float k_Pi = 3.14159265f;

float Sine(float degrees)
{
	return std::sin(degrees * k_Pi / 180.0f);
}
} // namespace

TEST(RealMoon, ItIsHalfADegreeAcross)
{
	const float radius = moon::Radius(atmos::k_MoonDistance);
	EXPECT_NEAR(2.0f * std::atan(radius / atmos::k_MoonDistance) * 180.0f / k_Pi, moon::k_ApparentDegrees, 1e-4f);
}

TEST(RealMoon, ItsFaceKeepsItsNorthToTheNorthOfTheSky)
{
	// Low in the south, the moon's north is up and a little tilted, and its east is on the west side of the sky
	const glm::vec3 south = -atmos::k_North;
	const auto face = moon::FaceAxes(glm::normalize(south + glm::vec3(0.0f, 0.2f, 0.0f)));
	EXPECT_GT(face.north.y, 0.9f);
	EXPECT_GT(glm::dot(face.east, -atmos::k_East), 0.99f);
	// The axes are square to each other and to the line from the eye
	EXPECT_NEAR(glm::dot(face.east, face.north), 0.0f, 1e-5f);
	EXPECT_NEAR(glm::dot(face.east, face.toEye), 0.0f, 1e-5f);
	EXPECT_NEAR(glm::dot(face.north, face.toEye), 0.0f, 1e-5f);
	// The pole of the sky is 40 degrees over the north horizon
	EXPECT_NEAR(std::asin(moon::SkyNorthPole().y) * 180.0f / k_Pi, atmos::k_LatitudeDegrees, 1e-3f);
	// Rising in the east, the moon's north leans to the left (the north of the sky), as at 40 degrees north
	const auto rising = moon::FaceAxes(glm::normalize(atmos::k_East + glm::vec3(0.0f, 0.05f, 0.0f)));
	EXPECT_GT(glm::dot(rising.north, atmos::k_North), 0.6f);
}

TEST(RealMoon, TheFaceThatTheEyeSeesIsTheMiddleOfTheMap)
{
	const auto face = moon::FaceAxes(glm::vec3(0.3f, 0.5f, -0.8f));
	const glm::vec2 middle = moon::MapPoint(face.toEye, face);
	EXPECT_NEAR(middle.x, 0.5f, 1e-5f);
	EXPECT_NEAR(middle.y, 0.5f, 1e-5f);
	// The moon's north pole is the top row, and its east limb is 90 degrees east
	EXPECT_NEAR(moon::MapPoint(face.north, face).y, 0.0f, 1e-4f);
	EXPECT_NEAR(moon::MapPoint(face.east, face).x, 0.75f, 1e-4f);
	EXPECT_NEAR(moon::MapPoint(-face.east, face).x, 0.25f, 1e-4f);
}

TEST(RealMoon, ItsDustLightsTheFullMoonEvenly)
{
	EXPECT_FLOAT_EQ(moon::SurfaceLight(1.0f, 1.0f), 1.0f);
	// At a full moon the edge is as bright as the middle
	EXPECT_FLOAT_EQ(moon::SurfaceLight(0.2f, 0.2f), 1.0f);
	// The night side is dark, and the light falls to the line between day and night
	EXPECT_FLOAT_EQ(moon::SurfaceLight(-0.1f, 0.8f), 0.0f);
	EXPECT_LT(moon::SurfaceLight(0.05f, 0.8f), moon::SurfaceLight(0.5f, 0.8f));
	// The earth lights the dark side most at a new moon and not at a full moon
	EXPECT_FLOAT_EQ(moon::EarthLight(1.0f), moon::k_Earthshine);
	EXPECT_FLOAT_EQ(moon::EarthLight(-1.0f), 0.0f);
	EXPECT_NEAR(moon::EarthLight(0.0f), 0.5f * moon::k_Earthshine, 1e-6f);
}

TEST(RealMoon, NearTheHorizonItIsDimmerAndRedder)
{
	const glm::vec3 high = moon::AirTint(1.0f);
	EXPECT_NEAR(glm::length(high - glm::vec3(1.0f)), 0.0f, 1e-4f);
	float last = 2.0f;
	for (const float degrees : {60.0f, 30.0f, 10.0f, 3.0f, 0.0f})
	{
		const glm::vec3 tint = moon::AirTint(Sine(degrees));
		EXPECT_LT(atmos::Luminance(tint), last);
		last = atmos::Luminance(tint);
		EXPECT_GE(tint.r, tint.g);
		EXPECT_GE(tint.g, tint.b);
	}
	// At the horizon it is red and dim, but still visible
	const glm::vec3 low = moon::AirTint(0.0f);
	EXPECT_GT(low.r, 4.0f * low.b);
	EXPECT_GT(atmos::Luminance(low), 0.05f);
	EXPECT_LT(atmos::Luminance(low), 0.4f);
	// The halo is stronger in the hazy air near the horizon, and grows with the moon's light
	EXPECT_GT(moon::HaloStrength(0.0f, 1.0f), moon::HaloStrength(1.0f, 1.0f));
	EXPECT_LT(moon::HaloStrength(0.5f, 0.2f), moon::HaloStrength(0.5f, 1.0f));
	// By night it hides the stars behind it; by day only its light adds to the sky
	EXPECT_FLOAT_EQ(moon::Cover(0.0f), moon::k_NightCover);
	EXPECT_FLOAT_EQ(moon::Cover(1.0f), 0.0f);
	EXPECT_FLOAT_EQ(moon::Cover(2.0f), 0.0f);
}
