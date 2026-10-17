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

#include <algorithm>
#include <array>
#include <numbers>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Graphics/Atmosphere.h"
#include "Graphics/Moon.h"
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

TEST(Atmosphere, TheExtraHazeIsInTheLowAirSoTheSeaShowsFromHighUp)
{
	EXPECT_FLOAT_EQ(atmos::HazeScaleAt(0.0f), atmos::k_HazeScale);
	EXPECT_GT(atmos::HazeScaleAt(0.3f), 0.85f * atmos::k_HazeScale);
	EXPECT_LT(atmos::HazeScaleAt(20.0f), 1.01f);
	EXPECT_GT(atmos::HazeScaleAt(1.0f), atmos::HazeScaleAt(2.0f));
	// Straight down from 20 km to the sea, more than half of the light comes through
	const auto sun = Towards(60.0f);
	const auto down = atmos::HazeAlong(glm::vec3(0.0f, -1.0f, 0.0f), sun, 20.0f, 20.0f * atmos::k_UnitsPerKilometre, k_Clear);
	EXPECT_GT(atmos::Luminance(down.through), 0.5f);
	// Along the sea over 25 km, almost nothing comes through, so the horizon is hidden
	const glm::vec3 along = glm::normalize(glm::vec3(-1.0f, 0.0f, 0.2f));
	const auto flat = atmos::HazeAlong(along, sun, 0.02f, 25.0f * atmos::k_UnitsPerKilometre, k_Clear);
	EXPECT_LT(atmos::Luminance(flat.through), 0.1f);
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

TEST(Atmosphere, TheLowSunsLightIsOrangeOnTheScreen)
{
	// The linear light of a sun 1 degree over the horizon is a deep red with almost no blue
	const atmos::Air air {.turbidity = 2.0f};
	const glm::vec3 linear = atmos::LightAtGround(std::sin(k_Pi / 180.0f), air);
	const glm::vec3 screen = atmos::OnScreen(linear);
	// On the screen it keeps its order of colours but has more green and blue for each red: orange, not red
	EXPECT_GT(screen.r, screen.g);
	EXPECT_GT(screen.g, screen.b);
	EXPECT_GT(screen.g / screen.r, 2.0f * linear.g / linear.r);
	EXPECT_EQ(atmos::OnScreen(glm::vec3(1.0f)), glm::vec3(1.0f));
	EXPECT_EQ(atmos::OnScreen(glm::vec3(-1.0f)), glm::vec3(0.0f));
}

TEST(Atmosphere, TheCloudsShadeTakesTheUpperSkyWhileTheSunIsLow)
{
	const glm::vec3 horizonRed {0.22f, 0.145f, 0.12f};
	const glm::vec3 upperBlue {0.1f, 0.1f, 0.16f};
	// By day the original colour stays
	EXPECT_EQ(atmos::CloudSkyLight(horizonRed, upperBlue, 0.5f), horizonRed);
	// With a low sun the light is blue-grey at the original's luminance
	const glm::vec3 dusk = atmos::CloudSkyLight(horizonRed, upperBlue, 0.02f);
	EXPECT_GT(dusk.b, dusk.r);
	EXPECT_NEAR(atmos::Luminance(dusk), atmos::Luminance(horizonRed), 1e-4f);
	// The dim sky after sunset has less colour
	const glm::vec3 deepBlue {0.0f, 0.1f, 1.0f};
	const glm::vec3 dim = atmos::DimSkyColour(deepBlue, -0.1f);
	EXPECT_NEAR(atmos::Luminance(dim), atmos::Luminance(deepBlue), 1e-4f);
	EXPECT_LT(dim.b - dim.r, 0.5f * (deepBlue.b - deepBlue.r));
	EXPECT_EQ(atmos::DimSkyColour(deepBlue, 0.5f), deepBlue);
}

TEST(Atmosphere, TheFarWorldCurvesAwayAndTheHorizonGoesDownWithTheHeight)
{
	constexpr float k_Map = 5120.0f;
	// From the middle of the map the flat reach is the distance to a corner
	EXPECT_NEAR(atmos::FlatReach({2560.0f, 2560.0f}, k_Map), 3620.4f, 0.1f);
	EXPECT_NEAR(atmos::FlatReach({0.0f, 0.0f}, k_Map), 7240.8f, 0.1f);
	// Nothing on the map goes down
	EXPECT_EQ(atmos::WorldDrop(3000.0f, 3620.0f), 0.0f);
	EXPECT_GT(atmos::WorldDrop(100000.0f, 3620.0f), 700.0f);
	// A ray that meets the sea inside the flat reach meets it where the flat sea is
	const glm::vec3 steep = glm::normalize(glm::vec3(1.0f, -0.5f, 0.0f));
	ASSERT_TRUE(atmos::CurvedSeaDistance(100.0f, steep, 3620.0f).has_value());
	EXPECT_NEAR(*atmos::CurvedSeaDistance(100.0f, steep, 3620.0f), 100.0f / 0.5f * std::sqrt(1.25f), 1e-2f);
	// The angle under the eye's level at which the rays stop to meet the sea
	const auto dip = [](float height) {
		float low = 0.0f;
		float high = 0.5f;
		for (int i = 0; i < 40; ++i)
		{
			const float mid = 0.5f * (low + high);
			const glm::vec3 ray {std::cos(mid), -std::sin(mid), 0.0f};
			(atmos::CurvedSeaDistance(height, ray, 3620.0f).has_value() ? high : low) = mid;
		}
		return high;
	};
	const float low = dip(50.0f);
	const float high = dip(30000.0f);
	EXPECT_GT(low, 0.0f);
	EXPECT_GT(high, 10.0f * low);
	// From high up the horizon is near the real planet's, under the eye's level by the angle acos(R / (R + h))
	EXPECT_NEAR(high, std::acos(atmos::k_WorldRadius / (atmos::k_WorldRadius + 30000.0f)), 0.002f);
	// The direction to the horizon is the same as the rays find
	EXPECT_NEAR(atmos::WorldHorizonY(30000.0f, 3620.0f), -std::sin(high), 0.002f);
	EXPECT_NEAR(atmos::WorldHorizonY(50.0f, 3620.0f), -std::sin(low), 0.002f);
	// A ray over the horizon never meets the sea
	EXPECT_FALSE(atmos::CurvedSeaDistance(30000.0f, glm::vec3(std::cos(0.05f), -std::sin(0.05f), 0.0f), 3620.0f).has_value());
}

TEST(Atmosphere, TheMoonStandsEastOfTheSunByItsPhase)
{
	const auto angle = [](glm::vec3 a, glm::vec3 b) {
		return std::acos(std::clamp(glm::dot(glm::normalize(a), glm::normalize(b)), -1.0f, 1.0f)) * 180.0f / k_Pi;
	};
	for (const float hour : {0.0f, 6.0f, 12.0f, 17.5f, 21.0f})
	{
		const auto sun = atmos::SunTowards(hour);
		// Next to the sun at a new moon, 90 degrees away at a quarter and opposite at a full moon
		EXPECT_NEAR(angle(atmos::MoonTowards(hour, 0.0), sun), 0.0f, 0.1f);
		EXPECT_NEAR(angle(atmos::MoonTowards(hour, 0.25), sun), 90.0f, 0.1f);
		EXPECT_NEAR(angle(atmos::MoonTowards(hour, 0.5), sun), 180.0f, 0.1f);
		EXPECT_NEAR(angle(atmos::MoonTowards(hour, 0.75), sun), 90.0f, 0.1f);
		EXPECT_NEAR(angle(atmos::MoonTowards(hour, 0.1), sun), 36.0f, 0.1f);
	}
	// A young moon is east of the sun: after sunset it is still up in the west
	const auto evening = atmos::MoonTowards(18.5f, 0.12);
	EXPECT_GT(evening.y, 0.2f);
	EXPECT_LT(glm::dot(evening, atmos::k_East), 0.0f);
	// At the spring equinox the first quarter is high and the last quarter low, as the moon goes along the sun's path
	EXPECT_GT(std::asin(atmos::MoonTowards(18.0f, 0.25).y) * 180.0f / k_Pi, 70.0f);
	EXPECT_LT(std::asin(atmos::MoonTowards(6.0f, 0.75).y) * 180.0f / k_Pi, 30.0f);
}

TEST(Atmosphere, TheMoonComesUpAboutFiftyMinutesLaterEachDay)
{
	// The first hour after noon at which the moon is over the horizon
	const auto rise = [](double fractionAtNoon) {
		constexpr double k_PerHour = 0.03386318012808897 / 24.0;
		for (int step = 1; step < 24 * 600; ++step)
		{
			const float hour = 12.0f + static_cast<float>(step) / 600.0f;
			const double fraction = fractionAtNoon + (k_PerHour * (static_cast<double>(hour) - 12.0));
			const float before = 12.0f + static_cast<float>(step - 1) / 600.0f;
			const double fractionBefore = fractionAtNoon + (k_PerHour * (static_cast<double>(before) - 12.0));
			if (atmos::MoonTowards(hour, fraction).y >= 0.0f && atmos::MoonTowards(before, fractionBefore).y < 0.0f)
			{
				return hour;
			}
		}
		return -1.0f;
	};
	const float today = rise(0.45);
	const float tomorrow = rise(0.45 + 0.03386318012808897);
	ASSERT_GT(today, 0.0f);
	ASSERT_GT(tomorrow, 0.0f);
	const float minutes = (tomorrow - today) * 60.0f;
	EXPECT_GT(minutes, 30.0f);
	EXPECT_LT(minutes, 75.0f);
}

TEST(Atmosphere, TheMoonsMonthMovesOnSmoothlyThroughTheDaysOfTheGame)
{
	// 15 March 2025, at noon
	constexpr int64_t k_Date = 1742040000;
	// At the start of the day it is the game's own moon month
	EXPECT_NEAR(atmos::MoonMonthFraction(k_Date, 0, 0.0f), openblack::graphics::moon::MonthFraction(k_Date), 1e-9);
	EXPECT_NEAR(atmos::MoonPhase(atmos::MoonMonthFraction(k_Date, 0, 0.0f)), openblack::graphics::moon::Phase(k_Date), 1e-5f);
	// Midnight at the end of a day of the game is the start of the next one
	EXPECT_NEAR(atmos::MoonMonthFraction(k_Date, 0, 24.0f), atmos::MoonMonthFraction(k_Date, 1, 0.0f), 1e-9);
	// A day is a 29.53th of a moon month
	const double day = atmos::MoonMonthFraction(k_Date, 3, 6.0f) - atmos::MoonMonthFraction(k_Date, 2, 6.0f);
	EXPECT_NEAR(day - std::floor(day), 0.03386318012808897, 1e-9);
	EXPECT_FLOAT_EQ(atmos::MoonPhase(0.5), k_Pi);
	EXPECT_NEAR(atmos::MoonPhase(0.0), 2.0f * k_Pi, 1e-6f);
	// The days of the game start as the hour goes past midnight
	EXPECT_EQ(atmos::DaysStarted(-1.0f, 0.1f), 0);
	EXPECT_EQ(atmos::DaysStarted(23.9f, 0.1f), 1);
	EXPECT_EQ(atmos::DaysStarted(0.1f, 23.9f), -1);
	EXPECT_EQ(atmos::DaysStarted(11.0f, 11.1f), 0);
	EXPECT_EQ(atmos::DaysStarted(5.0f, 19.0f), -1);
	EXPECT_EQ(atmos::DaysStarted(12.0f, 0.0f), 0);
}

TEST(Atmosphere, TheMoonsLightIsWeakerLowDown)
{
	const auto sine = [](float degrees) { return std::sin(degrees * k_Pi / 180.0f); };
	EXPECT_NEAR(atmos::MoonThroughAir(1.0f), 1.0f, 1e-3f);
	EXPECT_FLOAT_EQ(atmos::MoonThroughAir(-0.1f), 0.0f);
	EXPECT_FLOAT_EQ(atmos::MoonThroughAir(0.0f), 0.0f);
	float last = 0.0f;
	for (const float degrees : {1.0f, 5.0f, 10.0f, 30.0f, 60.0f, 90.0f})
	{
		const float through = atmos::MoonThroughAir(sine(degrees));
		EXPECT_GT(through, last);
		last = through;
	}
	EXPECT_NEAR(atmos::MoonThroughAir(sine(30.0f)), std::exp(-0.2f), 0.01f);
}

TEST(Atmosphere, TheNightAirTakesItsLightFromTheMoonWhileItIsUp)
{
	const auto sun = atmos::SunTowards(0.0f);
	const auto up = atmos::MoonTowards(0.0f, 0.5);
	EXPECT_NEAR(glm::length(atmos::AirMoonTowards(sun, up) - up), 0.0f, 1e-4f);
	// A new moon is down with the sun: the air keeps the glow from the point opposite the sun
	const auto down = atmos::MoonTowards(0.0f, 0.02);
	ASSERT_LT(down.y, -0.1f);
	EXPECT_NEAR(glm::length(atmos::AirMoonTowards(sun, down) + sun), 0.0f, 1e-4f);
}

TEST(Atmosphere, TheNightSkyGivesAFaintLightAfterSunset)
{
	const auto sine = [](float degrees) { return std::sin(degrees * k_Pi / 180.0f); };
	EXPECT_FLOAT_EQ(atmos::NightGlow(sine(30.0f)), 0.0f);
	EXPECT_FLOAT_EQ(atmos::NightGlow(sine(-1.0f)), 0.0f);
	EXPECT_NEAR(atmos::NightGlow(sine(-5.0f)), 0.5f * atmos::k_NightGlow, 1e-3f);
	EXPECT_FLOAT_EQ(atmos::NightGlow(sine(-30.0f)), atmos::k_NightGlow);
}

TEST(Atmosphere, TheIslandSitsUnderTheAurorasOvalOnStrongNights)
{
	// The island is at a magnetic latitude of 58 degrees, and the pole is to the north
	EXPECT_NEAR(atmos::MagneticLatitude({0.0f, 1.0f, 0.0f}), 58.0f, 1e-3f);
	EXPECT_GT(glm::dot(atmos::MagneticPole(), atmos::k_North), 0.5f);
	// The strength of each night is from 0 to 1: most nights are weak and some are strong
	float sum = 0.0f;
	int strong = 0;
	for (int night = 0; night < 1000; ++night)
	{
		const float strength = atmos::AuroraStrength(night);
		EXPECT_GE(strength, 0.0f);
		EXPECT_LE(strength, 1.0f);
		sum += strength;
		strong += strength > 0.8f ? 1 : 0;
	}
	EXPECT_LT(sum / 1000.0f, 0.45f);
	EXPECT_GT(strong, 50);
	// The same night always has the same strength
	EXPECT_EQ(atmos::AuroraStrength(7), atmos::AuroraStrength(7));
}
