/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The vortex's fade and land curves (docs/bw1-notes/vortex.md): FadeValue, LandFactorValue and LandOffset with the
// retail spline (every y 0). The values are exact in float.

#include <gtest/gtest.h>

#include "ECS/Vortex.h"

using openblack::VortexStateType;
namespace vortex = openblack::ecs::vortex;

TEST(Vortex, Fade)
{
	EXPECT_EQ(vortex::FadeValue(VortexStateType::Inactive, 3.0f), 0.0f);
	EXPECT_EQ(vortex::FadeValue(VortexStateType::Active, 0.0f), 1.0f);
	// FadeIn: 0 for 2 s, then smooth((e - 2) / 5)
	EXPECT_EQ(vortex::FadeValue(VortexStateType::FadeIn, 1.9f), 0.0f);
	EXPECT_EQ(vortex::FadeValue(VortexStateType::FadeIn, 2.0f), 0.0f);
	EXPECT_EQ(vortex::FadeValue(VortexStateType::FadeIn, 4.5f), 0.5f); // (3 - 1) 0.5 0.5
	EXPECT_EQ(vortex::FadeValue(VortexStateType::FadeIn, 7.0f), 1.0f);
	EXPECT_EQ(vortex::FadeValue(VortexStateType::FadeIn, 20.0f), 1.0f);
	// FadeOut: smooth(1 - e / 5) for 5 s, then 0
	EXPECT_EQ(vortex::FadeValue(VortexStateType::FadeOut, 0.0f), 1.0f);
	EXPECT_EQ(vortex::FadeValue(VortexStateType::FadeOut, 2.5f), 0.5f);
	EXPECT_EQ(vortex::FadeValue(VortexStateType::FadeOut, 5.0f), 0.0f);
}

TEST(Vortex, LandFactor)
{
	EXPECT_EQ(vortex::LandFactorValue(VortexStateType::FadeIn, 4.5f), 0.75f); // 1 - (1 - 0.5)^2
	EXPECT_EQ(vortex::LandFactorValue(VortexStateType::FadeIn, 1.0f), 0.0f);
	EXPECT_EQ(vortex::LandFactorValue(VortexStateType::Active, 0.0f), 1.0f);
	EXPECT_EQ(vortex::LandFactorValue(VortexStateType::FadeOut, 4.9f), 1.0f); // FadeOut takes s = 1
}

TEST(Vortex, LandOffset)
{
	// within 50 m: (mean - alt0) q
	EXPECT_EQ(vortex::LandOffset({30.0f, 0.0f}, 10.0f, 20.0f, 0.75f), 7.5f);
	EXPECT_EQ(vortex::LandOffset({0.0f, 0.0f}, 30.0f, 20.0f, 1.0f), -10.0f);
	// 50..56 m: (56 - r)(mean - alt0) / 6 q
	EXPECT_EQ(vortex::LandOffset({0.0f, 53.0f}, 10.0f, 22.0f, 0.5f), 3.0f); // 3 x 12 / 6 x 0.5
	EXPECT_EQ(vortex::LandOffset({56.0f, 0.0f}, 10.0f, 22.0f, 1.0f), 0.0f);
	// beyond 56 m: 0
	EXPECT_EQ(vortex::LandOffset({40.0f, 50.0f}, 10.0f, 22.0f, 1.0f), 0.0f);
}
