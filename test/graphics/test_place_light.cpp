/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the light of each place on the planet's ball by the height of its own sun

#include <cmath>

#include <algorithm>
#include <array>
#include <numbers>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Graphics/PlaceLight.h"

using namespace openblack::graphics::place_light;

namespace
{
float SineOf(float degrees)
{
	return std::sin(degrees * std::numbers::pi_v<float> / 180.0f);
}

// A fake light: the sun is full over 4 degrees and gone under -1, the glow comes under -2, the sky is brighter by day
Inputs FakeInputs()
{
	return {
	    .sunColour = {1.0f, 0.9f, 0.8f},
	    .sunStrength =
	        [](float height) { return std::clamp((height - SineOf(-1.0f)) / (SineOf(4.0f) - SineOf(-1.0f)), 0.0f, 1.0f); },
	    .nightGlow = [](float height) { return height < SineOf(-2.0f) ? 0.12f : 0.0f; },
	    .skyColour = [](float height) { return glm::vec3(0.1f + std::max(height, 0.0f)); },
	    .airTint = [](float height) { return glm::vec3(1.0f, 0.5f + 0.5f * std::max(height, 0.0f), 0.5f); },
	};
}

std::array<Knot, k_Knots> FakeTable()
{
	std::array<Knot, k_Knots> knots {};
	const auto inputs = FakeInputs();
	for (int i = 0; i < k_Knots; ++i)
	{
		knots[static_cast<size_t>(i)] = KnotAt(SineOf(KnotDegrees(i)), inputs);
	}
	return knots;
}
} // namespace

TEST(PlaceLight, KnotsRunFromUnderTheHorizonToStraightUp)
{
	EXPECT_FLOAT_EQ(KnotDegrees(0), -21.0f);
	EXPECT_FLOAT_EQ(KnotDegrees(7), 0.0f);
	EXPECT_FLOAT_EQ(KnotDegrees(16), 27.0f);
	EXPECT_FLOAT_EQ(KnotDegrees(17), 90.0f);
}

TEST(PlaceLight, PlaceOfFindsTheKnotsAroundAHeight)
{
	const auto low = PlaceOf(-1.0f);
	EXPECT_EQ(low.knot, 0);
	EXPECT_FLOAT_EQ(low.share, 0.0f);
	const auto horizon = PlaceOf(0.0f);
	EXPECT_EQ(horizon.knot, 7);
	EXPECT_NEAR(horizon.share, 0.0f, 1e-4f);
	const auto between = PlaceOf(SineOf(1.5f));
	EXPECT_EQ(between.knot, 7);
	EXPECT_NEAR(between.share, 0.5f, 1e-3f);
	const auto top = PlaceOf(1.0f);
	EXPECT_EQ(top.knot, 16);
	EXPECT_FLOAT_EQ(top.share, 1.0f);
}

TEST(PlaceLight, DaySideIsLitAndNightSideHasOnlyTheGlow)
{
	const auto knots = FakeTable();
	const auto day = Interpolate(knots, SineOf(40.0f));
	const auto night = Interpolate(knots, SineOf(-30.0f));
	EXPECT_NEAR(day.sunWater.r, 1.0f, 1e-4f);
	EXPECT_FLOAT_EQ(day.glow, 0.0f);
	EXPECT_FLOAT_EQ(night.sunWater.r, 0.0f);
	EXPECT_FLOAT_EQ(night.glow, 0.12f);
	EXPECT_GT(day.sky.r, night.sky.r);
}

TEST(PlaceLight, InterpolateMatchesTheKnotsAtTheirHeights)
{
	const auto knots = FakeTable();
	for (int i = 0; i < k_Knots; ++i)
	{
		const auto at = Interpolate(knots, SineOf(KnotDegrees(i)));
		EXPECT_NEAR(glm::distance(at.sky, knots[static_cast<size_t>(i)].sky), 0.0f, 1e-4f) << i;
		EXPECT_NEAR(glm::distance(at.sunWater, knots[static_cast<size_t>(i)].sunWater), 0.0f, 1e-4f) << i;
	}
}

TEST(PlaceLight, PackPutsFourVectorsForEachKnot)
{
	auto knots = FakeTable();
	knots[3].airScale = glm::vec3(2.0f, 3.0f, 4.0f);
	const auto packed = Pack(knots);
	EXPECT_FLOAT_EQ(packed[(3 * k_VectorsPerKnot) + 3].y, 3.0f);
	EXPECT_FLOAT_EQ(packed[(0 * k_VectorsPerKnot) + 0].w, knots[0].glow);
	EXPECT_FLOAT_EQ(packed[(16 * k_VectorsPerKnot) + 1].x, knots[16].sky.x);
}

TEST(PlaceLight, IslandShareFadesBetween200And1000Kilometres)
{
	EXPECT_FLOAT_EQ(IslandShare(1.0f), 1.0f);
	EXPECT_FLOAT_EQ(IslandShare(std::cos(k_IslandNear)), 1.0f);
	EXPECT_FLOAT_EQ(IslandShare(std::cos(k_IslandFar)), 0.0f);
	EXPECT_FLOAT_EQ(IslandShare(-1.0f), 0.0f);
	const float middle = IslandShare(std::cos(0.5f * (k_IslandNear + k_IslandFar)));
	EXPECT_GT(middle, 0.0f);
	EXPECT_LT(middle, 1.0f);
}

TEST(PlaceLight, MoonStrengthKeepsTheGlowWhenTheMoonIsDown)
{
	EXPECT_FLOAT_EQ(MoonStrengthAt(0.8f, -0.5f, 0.1f), 0.1f);
	EXPECT_FLOAT_EQ(MoonStrengthAt(0.8f, 0.9f, 0.1f), 0.8f);
	EXPECT_LT(MoonStrengthAt(0.8f, 0.05f, 0.0f), 0.8f);
}
