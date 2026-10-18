/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the Milky Way and the faint nebulae

#include <cmath>

#include <numbers>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Graphics/Galaxy.h"

namespace galaxy = openblack::graphics::galaxy;

namespace
{
/// A direction on the sky's sphere from a right ascension and a declination in degrees
glm::vec3 Sky(float rightAscension, float declination)
{
	const float a = rightAscension * std::numbers::pi_v<float> / 180.0f;
	const float d = declination * std::numbers::pi_v<float> / 180.0f;
	return {std::cos(d) * std::cos(a), std::cos(d) * std::sin(a), std::sin(d)};
}
} // namespace

TEST(Galaxy, TheMiddleAndThePoleOfTheGalaxyAreAtTheirPlaces)
{
	const glm::vec3 middle = galaxy::GalacticDirection(Sky(266.405f, -28.936f));
	EXPECT_NEAR(middle.x, 1.0f, 1e-5f);
	EXPECT_NEAR(middle.y, 0.0f, 1e-4f);
	EXPECT_NEAR(middle.z, 0.0f, 1e-4f);
	const glm::vec3 pole = galaxy::GalacticDirection(Sky(192.859508f, 27.128336f));
	EXPECT_NEAR(pole.z, 1.0f, 1e-6f);
	// The turn keeps the length of a direction
	EXPECT_NEAR(glm::length(galaxy::GalacticDirection(Sky(40.0f, 10.0f))), 1.0f, 1e-5f);
}

TEST(Galaxy, KnownStarsHaveTheirGalacticLongitudeAndLatitude)
{
	// Deneb: l 84.28, b 1.99
	const glm::vec2 deneb = galaxy::LongitudeLatitude(galaxy::GalacticDirection(Sky(310.358f, 45.280f)));
	EXPECT_NEAR(deneb.x, 84.28f, 0.05f);
	EXPECT_NEAR(deneb.y, 2.00f, 0.05f);
	// The Orion Nebula: l 209.01, b -19.38 (-150.99 on the scale of -180 to 180)
	const glm::vec2 orion = galaxy::LongitudeLatitude(galaxy::GalacticDirection(Sky(83.822f, -5.391f)));
	EXPECT_NEAR(orion.x, 209.01f - 360.0f, 0.05f);
	EXPECT_NEAR(orion.y, -19.38f, 0.05f);
}

TEST(Galaxy, TheBandIsBrightestAtTheMiddleAndFaintOppositeIt)
{
	const float middle = galaxy::BandBrightness({0.0f, -3.0f});
	const float opposite = galaxy::BandBrightness({180.0f, 0.0f});
	const float cygnus = galaxy::BandBrightness({75.0f, 0.0f});
	EXPECT_GT(middle, 1.5f);
	EXPECT_LT(opposite, 0.3f);
	EXPECT_GT(cygnus, opposite * 2.0f);
	EXPECT_LT(galaxy::BandBrightness({0.0f, 80.0f}), 0.01f);
}

TEST(Galaxy, TheDustIsDarkInTheRiftAndTheCoalsack)
{
	EXPECT_GT(galaxy::DustDepth({-58.5f, -1.0f}), 2.0f);
	EXPECT_GT(galaxy::DustDepth({30.0f, 2.0f}), 1.0f);
	EXPECT_LT(galaxy::DustDepth({0.0f, -6.0f}), 0.2f);
	EXPECT_LT(galaxy::DustDepth({120.0f, 40.0f}), 0.01f);
}

TEST(Galaxy, TheFaintStarsAreDenserOnTheBand)
{
	const float pole = galaxy::StarDensity(Sky(192.859508f, 27.128336f));
	EXPECT_NEAR(pole, 1.0f, 0.02f);
	// The star cloud of Sagittarius, a little south of the middle of the galaxy
	const float sagittarius = galaxy::StarDensity(Sky(271.0f, -30.0f));
	EXPECT_GT(sagittarius, 3.0f);
	EXPECT_LT(sagittarius, 4.5f);
}

TEST(Galaxy, TheBandShowsOnlyInADarkSky)
{
	EXPECT_FLOAT_EQ(galaxy::Visibility(0.0f), 1.0f);
	EXPECT_FLOAT_EQ(galaxy::Visibility(galaxy::k_FullVisibilityLuminance), 1.0f);
	EXPECT_FLOAT_EQ(galaxy::Visibility(galaxy::k_NoVisibilityLuminance), 0.0f);
	EXPECT_FLOAT_EQ(galaxy::Visibility(0.5f), 0.0f);
	// It fades steadily between the two
	float last = 1.0f;
	for (float luminance = 0.03f; luminance < galaxy::k_NoVisibilityLuminance; luminance += 0.005f)
	{
		const float now = galaxy::Visibility(luminance);
		EXPECT_LT(now, last);
		last = now;
	}
}

TEST(Galaxy, AFullMoonHighInTheSkyCoversMostOfTheBand)
{
	EXPECT_FLOAT_EQ(galaxy::MoonVisibility(0.0f, 1.0f), 1.0f);
	EXPECT_FLOAT_EQ(galaxy::MoonVisibility(1.0f, -0.2f), 1.0f);
	EXPECT_NEAR(galaxy::MoonVisibility(1.0f, 0.8f), 1.0f - galaxy::k_MoonWash, 1e-6f);
	EXPECT_LT(galaxy::MoonVisibility(1.0f, 0.8f), 0.2f);
	// A half moon covers less than a full moon
	EXPECT_GT(galaxy::MoonVisibility(0.5f, 0.8f), galaxy::MoonVisibility(1.0f, 0.8f));
}
