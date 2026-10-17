/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the light that the land and the sky give to the shade

#include <cmath>

#include <numbers>
#include <set>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Graphics/GlobalIllumination.h"

namespace gi = openblack::graphics::global_illumination;
namespace atmos = openblack::graphics::atmosphere;

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;

gi::ShL1 Project(uint16_t count, const auto& radianceOf)
{
	gi::ShL1 sh;
	for (const auto& direction : gi::RayDirections(count))
	{
		gi::ShAdd(sh, direction, radianceOf(direction), 4.0f * k_Pi / static_cast<float>(count));
	}
	return sh;
}
} // namespace

TEST(GlobalIllumination, TheRaysSpreadEvenly)
{
	const auto directions = gi::RayDirections(gi::k_RaysPerProbe);
	ASSERT_EQ(directions.size(), gi::k_RaysPerProbe);
	glm::vec3 mean {0.0f};
	for (const auto& direction : directions)
	{
		EXPECT_NEAR(glm::length(direction), 1.0f, 1e-4f);
		mean += direction;
	}
	EXPECT_LT(glm::length(mean / static_cast<float>(directions.size())), 0.02f);
}

TEST(GlobalIllumination, AnEvenSkyGivesPiTimesItsLight)
{
	const glm::vec3 radiance {0.2f, 0.4f, 0.8f};
	const auto sh = Project(512, [&](glm::vec3) { return radiance; });
	for (const glm::vec3 normal : {glm::vec3(0, 1, 0), glm::vec3(1, 0, 0), glm::vec3(0, -1, 0)})
	{
		const auto irradiance = gi::ShIrradiance(sh, normal);
		EXPECT_NEAR(irradiance.b, k_Pi * radiance.b, 0.02f);
		EXPECT_NEAR(irradiance.r, k_Pi * radiance.r, 0.01f);
	}
}

TEST(GlobalIllumination, LightFromAboveLightsWhatFacesUp)
{
	const auto sh = Project(512, [](glm::vec3 d) { return glm::vec3(d.y > 0.0f ? 1.0f : 0.0f); });
	const auto up = gi::ShIrradiance(sh, {0.0f, 1.0f, 0.0f});
	const auto side = gi::ShIrradiance(sh, {1.0f, 0.0f, 0.0f});
	const auto down = gi::ShIrradiance(sh, {0.0f, -1.0f, 0.0f});
	// A full upper hemisphere of 1 gives pi on flat ground and half of that on a wall; L1 holds that nearly
	EXPECT_NEAR(up.r, k_Pi, 0.35f);
	EXPECT_NEAR(side.r, k_Pi * 0.5f, 0.05f);
	EXPECT_LT(down.r, 0.35f);
	EXPECT_GE(down.r, 0.0f);
}

TEST(GlobalIllumination, TheSweepMakesEachRowOnceThenStops)
{
	gi::ProbeSweep sweep;
	EXPECT_FALSE(sweep.Next().has_value());
	sweep.Start();
	std::set<uint16_t> rows;
	int frames = 0;
	while (const auto band = sweep.Next())
	{
		++frames;
		for (uint16_t row = band->firstRow; row < band->firstRow + band->rows; ++row)
		{
			EXPECT_TRUE(rows.insert(row).second);
		}
	}
	EXPECT_EQ(rows.size(), gi::k_ProbesPerSide);
	EXPECT_EQ(frames, gi::k_ProbesPerSide / gi::k_RowsPerFrame);
	EXPECT_FALSE(sweep.Running());
	// A new start begins again at the first row
	sweep.Start();
	EXPECT_EQ(sweep.Next()->firstRow, 0);
}

TEST(GlobalIllumination, TheNewGridBlendsIn)
{
	EXPECT_EQ(gi::ProbeBlend(0.0f), 0.0f);
	EXPECT_EQ(gi::ProbeBlend(gi::k_BlendSeconds * 2.0f), 1.0f);
	EXPECT_GT(gi::ProbeBlend(1.0f), gi::ProbeBlend(0.5f));
	EXPECT_NEAR(gi::ProbeCentre(0, 63).x, gi::k_ProbeSpacing * 0.5f, 1e-3f);
	EXPECT_NEAR(gi::ProbeCentre(0, 63).y, gi::k_MapUnits - gi::k_ProbeSpacing * 0.5f, 1e-2f);
}

TEST(GlobalIllumination, TheShadeIsNeverBlackOrTooBright)
{
	const glm::vec3 sky {0.1f, 0.2f, 0.4f};
	const auto same = gi::ShadeTint(sky, sky);
	EXPECT_NEAR(atmos::Luminance(same), 1.0f, 1e-3f);
	const auto none = gi::ShadeTint(glm::vec3(0.0f), sky);
	EXPECT_NEAR(atmos::Luminance(none), gi::k_DarkestShade, 1e-3f);
	const auto lots = gi::ShadeTint(sky * 10.0f, sky);
	EXPECT_NEAR(atmos::Luminance(lots), gi::k_BrightestShade, 1e-3f);
	// Warm bounced light makes the shade warmer, but less than the light's own colour
	const glm::vec3 warm {0.5f, 0.3f, 0.1f};
	const auto tint = gi::ShadeTint(warm, glm::vec3(atmos::Luminance(warm)));
	EXPECT_GT(tint.r, tint.b);
	EXPECT_LT(tint.r / tint.b, warm.r / warm.b);
}

TEST(GlobalIllumination, TheDaySkyLightsTheGroundMoreThanTheUnderside)
{
	const glm::vec3 sun = glm::normalize(glm::vec3(0.3f, 0.8f, 0.2f));
	const auto sh = gi::SkySh(sun, 0.02f, atmos::Air {.turbidity = 2.0f}, 128);
	const auto up = gi::ShIrradiance(sh, {0.0f, 1.0f, 0.0f});
	const auto down = gi::ShIrradiance(sh, {0.0f, -1.0f, 0.0f});
	EXPECT_GT(atmos::Luminance(up), 4.0f * atmos::Luminance(down));
	EXPECT_GT(up.b, up.r);
}
