/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the volumetric fog

#include <gtest/gtest.h>

#include "Graphics/VolumetricFog.h"

namespace fog = openblack::graphics::volumetric_fog;

TEST(VolumetricFog, TheSlicesGoFartherApartAndInvert)
{
	EXPECT_NEAR(fog::SliceAt(fog::SliceDistance(0.0f)), 0.0f, 1e-4f);
	EXPECT_NEAR(fog::SliceAt(fog::SliceDistance(17.3f)), 17.3f, 1e-3f);
	// The far edge of the last slice is the farthest distance
	EXPECT_NEAR(fog::SliceDistance(static_cast<float>(fog::k_Slices) - 0.5f), fog::k_Farthest, 0.1f);
	// Far slices are thicker than near ones
	const float nearThickness = fog::SliceDistance(1.0f) - fog::SliceDistance(0.0f);
	const float farThickness = fog::SliceDistance(40.0f) - fog::SliceDistance(39.0f);
	EXPECT_GT(farThickness, nearThickness * 10.0f);
}

TEST(VolumetricFog, TheGroundFogComesAtDawnAndBurnsOff)
{
	// Thickest before and round sunrise, gone in the afternoon, a little at night
	EXPECT_GT(fog::DawnFog(5.5f), 0.9f);
	EXPECT_GT(fog::DawnFog(7.0f), 0.8f);
	EXPECT_LT(fog::DawnFog(9.5f), fog::DawnFog(8.0f));
	EXPECT_EQ(fog::DawnFog(14.0f), 0.0f);
	EXPECT_GT(fog::DawnFog(23.5f), 0.2f);
	EXPECT_LT(fog::DawnFog(23.5f), 0.5f);
	// It lifts as it thins
	EXPECT_GT(fog::FogTop(10.0f), fog::FogTop(6.0f) + 30.0f);
}

TEST(VolumetricFog, RainWetsTheLandAndAStormTakesTheGroundFogAway)
{
	float wetness = 0.0f;
	for (int i = 0; i < 600; ++i)
	{
		wetness = fog::StepWetness(wetness, 1.0f, 0.1f);
	}
	EXPECT_GT(wetness, 0.9f);
	// It dries over minutes, not seconds
	const float wet = wetness;
	for (int i = 0; i < 600; ++i)
	{
		wetness = fog::StepWetness(wetness, 0.0f, 0.1f);
	}
	EXPECT_GT(wetness, wet * 0.8f);
	// More fog on wet land; none in a storm, which thickens the haze instead
	EXPECT_GT(fog::FogFor(6.0f, 1.0f, 0.0f).groundDensity, fog::FogFor(6.0f, 0.0f, 0.0f).groundDensity * 2.0f);
	EXPECT_EQ(fog::FogFor(6.0f, 1.0f, 1.0f).groundDensity, 0.0f);
	EXPECT_GT(fog::FogFor(12.0f, 0.0f, 1.0f).hazeDensity, fog::FogFor(12.0f, 0.0f, 0.0f).hazeDensity * 3.0f);
}
