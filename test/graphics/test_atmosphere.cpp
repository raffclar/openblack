/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the air's light

#include <cmath>

#include <array>
#include <numbers>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Graphics/Atmosphere.h"
#include "Graphics/Underwater.h"

namespace atmos = openblack::graphics::atmosphere;

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;
const atmos::Air k_Clear {.turbidity = 1.0f};

glm::vec3 Towards(float elevationDegrees, float azimuthDegrees = 0.0f)
{
	const float e = elevationDegrees * k_Pi / 180.0f;
	const float a = azimuthDegrees * k_Pi / 180.0f;
	return {std::cos(e) * std::cos(a), std::sin(e), std::cos(e) * std::sin(a)};
}
} // namespace

TEST(Atmosphere, ThePhasesSpreadAllTheLight)
{
	// Each phase sums to 1 over the sphere
	constexpr int k_Steps = 4000;
	float rayleigh = 0.0f;
	float mie = 0.0f;
	for (int i = 0; i < k_Steps; ++i)
	{
		const float mu = -1.0f + (static_cast<float>(i) + 0.5f) * 2.0f / k_Steps;
		rayleigh += atmos::RayleighPhase(mu) * 2.0f * k_Pi * 2.0f / k_Steps;
		mie += atmos::MiePhase(mu, 0.5f) * 2.0f * k_Pi * 2.0f / k_Steps;
	}
	EXPECT_NEAR(rayleigh, 1.0f, 1e-3f);
	EXPECT_NEAR(mie, 1.0f, 1e-2f);
	// The dust sends most of its light on forward
	EXPECT_GT(atmos::MiePhase(1.0f, atmos::k_MieForward), 50.0f * atmos::MiePhase(-1.0f, atmos::k_MieForward));
}

TEST(Atmosphere, TheAirTakesMoreBlueThanRed)
{
	const auto up = atmos::Transmittance(0.0f, 1.0f, k_Clear);
	EXPECT_NEAR(up.r, 0.94f, 0.02f);
	EXPECT_NEAR(up.b, 0.76f, 0.02f);
	const auto low = atmos::Transmittance(0.0f, 0.05f, k_Clear);
	EXPECT_LT(low.b, up.b);
	EXPECT_LT(low.b, low.g);
	EXPECT_LT(low.g, low.r);
	// None gets through the ground, and more gets through from high up
	EXPECT_EQ(atmos::Transmittance(1.0f, -0.5f, k_Clear), glm::vec3(0.0f));
	EXPECT_GT(atmos::Transmittance(5.0f, 0.05f, k_Clear).b, low.b);
	// Dusty air takes more
	EXPECT_LT(atmos::Transmittance(0.0f, 0.3f, {.turbidity = 6.0f}).r, atmos::Transmittance(0.0f, 0.3f, k_Clear).r);
}

TEST(Atmosphere, TheLightIsWarmAndDimLowDown)
{
	const auto noon = atmos::LightAtGround(1.0f, k_Clear);
	const auto dusk = atmos::LightAtGround(std::sin(0.06f), k_Clear);
	EXPECT_GT(noon.b, dusk.b * 3.0f);
	EXPECT_GT(dusk.r, dusk.g);
	EXPECT_GT(dusk.g, dusk.b);
	EXPECT_EQ(atmos::LightAtGround(-0.1f, k_Clear), glm::vec3(0.0f));
}

TEST(Atmosphere, TheTransmittanceTextureMapsBothWays)
{
	for (const float altitude : {0.0f, 0.5f, 7.0f, 60.0f})
	{
		for (const float mu : {1.0f, 0.6f, 0.1f, 0.0f})
		{
			const auto uv = atmos::TransmittanceUv(altitude, mu);
			EXPECT_GE(uv.x, -1e-4f);
			EXPECT_LE(uv.x, 1.0001f);
			const auto back = atmos::FromTransmittanceUv(uv);
			EXPECT_NEAR(back.altitude, altitude, 2e-2f);
			EXPECT_NEAR(back.mu, mu, 2e-3f);
		}
	}
}

TEST(Atmosphere, TheSkyTextureMapsBothWaysAndFavoursTheHorizon)
{
	for (const float altitude : {0.01f, 2.0f})
	{
		for (const float viewMu : {0.95f, 0.5f, 0.05f, -0.2f, -0.8f})
		{
			for (const float lightCos : {1.0f, 0.3f, -0.6f})
			{
				const auto uv = atmos::SkyViewUv(viewMu, lightCos, altitude);
				const auto back = atmos::FromSkyViewUv(uv, altitude);
				EXPECT_NEAR(back.x, viewMu, 2e-3f);
				EXPECT_NEAR(back.y, lightCos, 2e-3f);
			}
		}
	}
	// The horizon is the middle row, and the rows near it span less of the sky than those near the zenith
	const float altitude = 0.01f;
	const float horizonMu = -std::sqrt(1.0f - std::pow(atmos::k_GroundRadius / (atmos::k_GroundRadius + altitude), 2.0f));
	EXPECT_NEAR(atmos::SkyViewUv(horizonMu, 1.0f, altitude).y, 0.5f, 5e-3f);
	const float nearHorizon = atmos::SkyViewUv(0.1f, 1.0f, altitude).y - atmos::SkyViewUv(0.0f, 1.0f, altitude).y;
	const float nearZenith = atmos::SkyViewUv(0.995f, 1.0f, altitude).y - atmos::SkyViewUv(0.9f, 1.0f, altitude).y;
	EXPECT_GT(std::abs(nearHorizon), std::abs(nearZenith));
	// Towards the light is the left edge
	EXPECT_NEAR(atmos::SkyViewUv(0.3f, 1.0f, altitude).x, 0.0f, 1e-4f);
	EXPECT_NEAR(atmos::SkyViewUv(0.3f, -1.0f, altitude).x, 1.0f, 1e-4f);
}

TEST(Atmosphere, TheHazeSlicesGoFartherApartAndInvert)
{
	EXPECT_NEAR(atmos::AerialSliceAt(atmos::AerialSliceDistance(5.3f)), 5.3f, 1e-3f);
	EXPECT_NEAR(atmos::AerialSliceDistance(static_cast<float>(atmos::k_AerialSlices) - 0.5f), atmos::k_AerialFarthest, 1.0f);
	EXPECT_GT(atmos::AerialSliceDistance(30.0f) - atmos::AerialSliceDistance(29.0f),
	          20.0f * (atmos::AerialSliceDistance(1.0f) - atmos::AerialSliceDistance(0.0f)));
}

TEST(Atmosphere, TheSkyIsBlueByDayWarmByTheLowSunAndDarkAtNight)
{
	const float altitude = 0.02f;
	const auto noonZenith = atmos::SkyRadiance({0.0f, 1.0f, 0.0f}, Towards(70.0f), altitude, k_Clear);
	EXPECT_GT(noonZenith.b, noonZenith.g);
	EXPECT_GT(noonZenith.g, noonZenith.r);
	// With the sun low, the horizon under it is warmer than the horizon opposite
	const auto sun = Towards(2.0f);
	const auto towards = atmos::SkyRadiance(Towards(3.0f), sun, altitude, k_Clear);
	const auto away = atmos::SkyRadiance(Towards(3.0f, 180.0f), sun, altitude, k_Clear);
	EXPECT_GT(towards.r / towards.b, away.r / away.b);
	EXPECT_GT(atmos::Luminance(towards), atmos::Luminance(away));
	// With the light well under the horizon the sky is far darker
	const auto night = atmos::SkyRadiance({0.0f, 1.0f, 0.0f}, Towards(-20.0f), altitude, k_Clear);
	EXPECT_LT(atmos::Luminance(night), 0.01f * atmos::Luminance(noonZenith));
}

TEST(Atmosphere, TheHazeThickensWithDistanceAndIsBlue)
{
	const auto sun = Towards(60.0f);
	const glm::vec3 view = glm::normalize(glm::vec3(-1.0f, 0.0f, 0.2f));
	const auto nearHaze = atmos::HazeAlong(view, sun, 0.02f, 300.0f, k_Clear);
	const auto farHaze = atmos::HazeAlong(view, sun, 0.02f, 6000.0f, k_Clear);
	EXPECT_GT(nearHaze.through.b, farHaze.through.b);
	EXPECT_LT(farHaze.through.b, farHaze.through.r);
	EXPECT_GT(farHaze.light.b, farHaze.light.r);
	EXPECT_GT(atmos::Luminance(farHaze.light), atmos::Luminance(nearHaze.light));
	// Higher up there is less of it over the same distance
	const auto high = atmos::HazeAlong(view, sun, 3.0f, 6000.0f, k_Clear);
	EXPECT_GT(high.through.b, farHaze.through.b);
}

TEST(Atmosphere, TheSunRisesInTheEastClimbsAtNoonAndSetsInTheWest)
{
	const auto degrees = [](glm::vec3 towards) { return std::asin(towards.y) * 180.0f / k_Pi; };
	// At an equinox the sun comes up due east at 6 and goes down due west at 18
	const auto rise = atmos::SunTowards(6.0f);
	EXPECT_NEAR(rise.y, 0.0f, 1e-4f);
	EXPECT_NEAR(glm::dot(rise, atmos::k_East), 1.0f, 1e-4f);
	const auto set = atmos::SunTowards(18.0f);
	EXPECT_NEAR(set.y, 0.0f, 1e-4f);
	EXPECT_NEAR(glm::dot(set, atmos::k_East), -1.0f, 1e-4f);
	// At noon it stands in the south at 90 degrees less the latitude
	const auto noon = atmos::SunTowards(12.0f);
	EXPECT_NEAR(degrees(noon), 90.0f - atmos::k_LatitudeDegrees, 1e-3f);
	EXPECT_NEAR(glm::dot(noon, atmos::k_North), -std::cos((90.0f - atmos::k_LatitudeDegrees) * k_Pi / 180.0f), 1e-4f);
	// Low in the morning, higher to noon, then down again by the same path
	EXPECT_GT(degrees(atmos::SunTowards(8.0f)), 10.0f);
	EXPECT_LT(degrees(atmos::SunTowards(8.0f)), degrees(atmos::SunTowards(10.0f)));
	EXPECT_NEAR(degrees(atmos::SunTowards(9.0f)), degrees(atmos::SunTowards(15.0f)), 1e-3f);
	EXPECT_NEAR(degrees(atmos::SunTowards(0.0f)), -(90.0f - atmos::k_LatitudeDegrees), 1e-3f);
	EXPECT_NEAR(glm::length(atmos::SunTowards(26.0f) - atmos::SunTowards(2.0f)), 0.0f, 1e-5f);
	// The morning sun is in the east half of the sky and the afternoon sun in the west half
	EXPECT_GT(glm::dot(atmos::SunTowards(9.0f), atmos::k_East), 0.0f);
	EXPECT_LT(glm::dot(atmos::SunTowards(15.0f), atmos::k_East), 0.0f);
}

TEST(Atmosphere, TheSunShowsOnlyNearAndOverTheHorizon)
{
	const auto sine = [](float degrees) { return std::sin(degrees * k_Pi / 180.0f); };
	EXPECT_FLOAT_EQ(atmos::SunStrength(sine(-2.0f)), 0.0f);
	EXPECT_FLOAT_EQ(atmos::SunStrength(sine(-1.0f)), 0.0f);
	EXPECT_NEAR(atmos::SunStrength(sine(1.5f)), 127.5f, 0.5f);
	EXPECT_FLOAT_EQ(atmos::SunStrength(sine(4.0f)), 255.0f);
	EXPECT_FLOAT_EQ(atmos::SunStrength(sine(50.0f)), 255.0f);
	// The moon takes the air's light only after the sun has gone under the horizon
	EXPECT_FLOAT_EQ(atmos::AirMoonShare(sine(2.0f)), 0.0f);
	EXPECT_FLOAT_EQ(atmos::AirMoonShare(sine(-2.0f)), 0.0f);
	EXPECT_NEAR(atmos::AirMoonShare(sine(-5.0f)), 0.5f, 1e-3f);
	EXPECT_FLOAT_EQ(atmos::AirMoonShare(sine(-8.0f)), 1.0f);
}

TEST(Atmosphere, TheAirsLightMovesRoundTheSouthFromTheSunToTheMoon)
{
	const auto sun = atmos::SunTowards(18.2f);
	const auto moon = -sun;
	EXPECT_NEAR(glm::length(atmos::AirLightTowards(sun, moon, 0.0f) - sun), 0.0f, 1e-4f);
	EXPECT_NEAR(glm::length(atmos::AirLightTowards(sun, moon, 1.0f) - moon), 0.0f, 1e-4f);
	// Half way it is under the horizon in the south, not at the top of the sky
	const auto half = atmos::AirLightTowards(sun, moon, 0.5f);
	EXPECT_NEAR(glm::length(half), 1.0f, 1e-4f);
	EXPECT_NEAR(half.y, -std::sin(10.0f * k_Pi / 180.0f), 0.01f);
	EXPECT_LT(glm::dot(half, atmos::k_North), -0.97f);
	// Small steps of the share give small turns
	for (int i = 0; i < 100; ++i)
	{
		const auto a = atmos::AirLightTowards(sun, moon, static_cast<float>(i) / 100.0f);
		const auto b = atmos::AirLightTowards(sun, moon, static_cast<float>(i + 1) / 100.0f);
		EXPECT_GT(glm::dot(a, b), std::cos(3.0f * k_Pi / 180.0f));
	}
}

TEST(Atmosphere, TheExposureMatchesTheTargetsBrightness)
{
	const glm::vec3 horizon {0.02f, 0.04f, 0.08f};
	const glm::vec3 target {0.6f, 0.7f, 0.8f};
	EXPECT_NEAR(atmos::Luminance(horizon * atmos::Exposure(horizon, target)), atmos::Luminance(target), 1e-4f);
	EXPECT_GT(atmos::AirFor(1.0f, 0.0f).turbidity, atmos::AirFor(0.0f, 0.0f).turbidity);
	EXPECT_GT(atmos::AirFor(0.0f, 1.0f).turbidity, atmos::AirFor(0.0f, 0.0f).turbidity);
	EXPECT_NEAR(atmos::AltitudeOf(949.0f), 1.0f, 1e-4f);
	// The horizon on the screen is as bright as the target, and with all of the target's colour it is the target
	const glm::vec3 tint {1.0f, 0.9f, 0.8f};
	const auto scale = atmos::ScreenScale(horizon, target, tint, atmos::k_OriginalHue);
	EXPECT_NEAR(atmos::Luminance(horizon * scale), atmos::Luminance(target), 1e-3f);
	const auto all = horizon * atmos::ScreenScale(horizon, target, tint, 1.0f);
	EXPECT_NEAR(all.r, target.r, 1e-4f);
	EXPECT_NEAR(all.b, target.b, 1e-4f);
}

TEST(Atmosphere, EachTierKeepsAHazeAndALightInTheShade)
{
	namespace underwater = openblack::graphics::underwater;
	using underwater::Effect;
	using underwater::LightDetail;
	using underwater::Quality;
	EXPECT_EQ(underwater::HazeDetail(Quality::High), LightDetail::Full);
	EXPECT_EQ(underwater::HazeDetail(Quality::Medium), LightDetail::Reduced);
	EXPECT_EQ(underwater::HazeDetail(Quality::Low), LightDetail::Basic);
	EXPECT_EQ(underwater::LightingDetail(Quality::Low), LightDetail::Basic);
	// The haze and the light stay on in every tier; only the shade from the scene's depth needs the high tier
	for (const auto quality : {Quality::High, Quality::Medium, Quality::Low})
	{
		EXPECT_TRUE(underwater::InQuality(Effect::Haze, quality));
		EXPECT_TRUE(underwater::InQuality(Effect::GlobalIllumination, quality));
		EXPECT_TRUE(underwater::InQuality(Effect::FogPockets, quality));
	}
	EXPECT_FALSE(underwater::InQuality(Effect::AmbientOcclusion, Quality::Medium));
}

TEST(Atmosphere, APlaceTintsItsHazeWithItsOwnColour)
{
	EXPECT_EQ(atmos::PlaceTint({}, atmos::k_PlaceShare), glm::vec3(1.0f));
	const std::array<glm::vec3, 2> grass {glm::vec3(0.2f, 0.5f, 0.1f), glm::vec3(0.3f, 0.6f, 0.2f)};
	const auto green = atmos::PlaceTint(grass, atmos::k_PlaceShare);
	EXPECT_GT(green.g, green.r);
	EXPECT_GT(green.g, green.b);
	EXPECT_NEAR(atmos::Luminance(green), 1.0f, 1e-3f);
	// With no share the haze keeps no colour of the place
	EXPECT_EQ(atmos::PlaceTint(grass, 0.0f), glm::vec3(1.0f));
}

TEST(Atmosphere, TheSkysTypeFollowsTheSunsHeight)
{
	const auto sine = [](float degrees) { return std::sin(degrees * k_Pi / 180.0f); };
	EXPECT_FLOAT_EQ(atmos::SkyTypeFromSun(sine(-40.0f)), 0.0f);
	EXPECT_FLOAT_EQ(atmos::SkyTypeFromSun(sine(-18.0f)), 0.0f);
	EXPECT_NEAR(atmos::SkyTypeFromSun(sine(-9.0f)), 0.5f, 1e-4f);
	EXPECT_NEAR(atmos::SkyTypeFromSun(sine(0.0f)), 1.0f, 1e-4f);
	EXPECT_NEAR(atmos::SkyTypeFromSun(sine(3.0f)), 1.0f, 1e-4f);
	EXPECT_NEAR(atmos::SkyTypeFromSun(sine(7.5f)), 1.5f, 1e-4f);
	EXPECT_FLOAT_EQ(atmos::SkyTypeFromSun(sine(10.0f)), 2.0f);
	// By 8 in the morning the day's sky shows
	EXPECT_FLOAT_EQ(atmos::SkyTypeFromSun(atmos::SunTowards(8.0f).y), 2.0f);
}
