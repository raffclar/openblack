/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "3D/TemplePool.h"

using namespace openblack;

TEST(TemplePool, TheFirstLayersAlphaFollowsTheSineAndCutsTheFraction)
{
	EXPECT_EQ(temple_pool::FirstLayerAlpha(0.0f), 128);
	EXPECT_EQ(temple_pool::FirstLayerAlpha(1.5707964f), 160);
	EXPECT_EQ(temple_pool::FirstLayerAlpha(-1.5707964f), 96);
	// 128 + 3.19 and 128 - 3.19: the cast removes the fraction on each side
	EXPECT_EQ(temple_pool::FirstLayerAlpha(0.1f), 131);
	EXPECT_EQ(temple_pool::FirstLayerAlpha(-0.1f), 124);
}

TEST(TemplePool, TheSecondLayerHasWhatTheFirstDoesNotHave)
{
	EXPECT_EQ(temple_pool::SecondLayerAlpha(160), 95);
	EXPECT_EQ(temple_pool::SecondLayerAlpha(96), 159);
}

TEST(TemplePool, TheLayersTakeThirteenSixteenthsOfEachByteOfTheLight)
{
	EXPECT_EQ(temple_pool::LayerLight(TempleLight::At(0.0f, 0)), glm::vec3(207.0f / 255.0f));
	// The evil light is 255, 175 and 160
	const auto evil = temple_pool::LayerLight(TempleLight::At(-1.0f, 0));
	EXPECT_EQ(evil.r, 207.0f / 255.0f);
	EXPECT_EQ(evil.g, 142.0f / 255.0f);
	EXPECT_EQ(evil.b, 130.0f / 255.0f);
}
