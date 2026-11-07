/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Graphics/ModelLight.h"
#include "Graphics/TreeBrightness.h"

namespace tree_brightness = openblack::tree_brightness;
using openblack::model_light::k_Sun;

TEST(TreeBrightness, FacingTheSunIsTheLeast)
{
	EXPECT_EQ(tree_brightness::Factor({0.0f, 0.0f, 0.0f}, {-1.0f, -0.5f, -1.0f}, k_Sun), 200);
	EXPECT_EQ(tree_brightness::Factor({0.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, k_Sun), 200);
}

TEST(TreeBrightness, TheSunBehindIsTheMost)
{
	// The light's direction past the focus is (1, -1, 1) / sqrt(3); the facing on the ground (1, 0, 1) / sqrt(2) meets
	// it at sqrt(2 / 3)
	EXPECT_EQ(tree_brightness::Factor({0.0f, 0.0f, 0.0f}, {1.0f, -0.3f, 1.0f}, k_Sun), 244);
	// Along x alone, 1 / sqrt(3)
	EXPECT_EQ(tree_brightness::Factor({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, k_Sun), 231);
}

TEST(TreeBrightness, ALightOverheadLightsNoMore)
{
	EXPECT_EQ(tree_brightness::Factor({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 100.0f, 0.0f}), 200);
}

TEST(TreeBrightness, TheLengthAddsZThenYThenX)
{
	// The game adds the squares of z, y and x in that order and multiplies by the inverse of the length. A sum in a
	// different order, or a division, gives 245 here.
	EXPECT_EQ(tree_brightness::Factor({4859.0f, 155.0f, 2035.0f}, {0x1.0195c8p-1f, 0x1.a8c3aep-7f, 0x1.0d79e0p-1f}, k_Sun),
	          244);
}
