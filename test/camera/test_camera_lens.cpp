/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>

#include <glm/trigonometric.hpp>
#include <gtest/gtest.h>

#include "Camera/CameraLens.h"

using namespace openblack::camera_lens;

namespace
{
/// The game's world lens: 70 degrees across a 4:3 screen
const float k_Lens = glm::radians(70.0f);
} // namespace

// On a 4:3 screen the lens spans the width and the view up and down is the game's own: 55.42 degrees
TEST(CameraLens, AFourByThreeScreenSeesTheGamesView)
{
	for (const float aspect : {640.0f / 480.0f, 800.0f / 600.0f, 1024.0f / 768.0f, 1600.0f / 1200.0f})
	{
		EXPECT_FLOAT_EQ(HorizontalFieldOfView(k_Lens, aspect), k_Lens) << aspect;
		EXPECT_NEAR(glm::degrees(VerticalFieldOfView(k_Lens, aspect)), 55.42f, 0.01f) << aspect;
	}
}

// A narrower screen (5:4) keeps the game's own rule: the lens across its width, a taller view up and down
TEST(CameraLens, ANarrowerScreenKeepsTheLensAcrossItsWidth)
{
	const float aspect = 1280.0f / 1024.0f;
	EXPECT_FLOAT_EQ(HorizontalFieldOfView(k_Lens, aspect), k_Lens);
	const float vanillaUp = 2.0f * std::atan(std::tan(k_Lens / 2.0f) / aspect);
	EXPECT_FLOAT_EQ(VerticalFieldOfView(k_Lens, aspect), vanillaUp);
	EXPECT_NEAR(glm::degrees(VerticalFieldOfView(k_Lens, aspect)), 58.5f, 0.05f);
}

// Wider screens keep the 4:3 view up and down and see further to the sides: about 86 degrees across at 16:9, 124 at
// 32:9
TEST(CameraLens, AWiderScreenKeepsTheFourByThreeViewUpAndDown)
{
	const float upAt4By3 = VerticalFieldOfView(k_Lens, 4.0f / 3.0f);
	struct Case
	{
		float aspect;
		float acrossDegrees;
	};
	for (const auto& [aspect, acrossDegrees] : std::array {Case {16.0f / 9.0f, 86.07f}, Case {2560.0f / 1440.0f, 86.07f},
	                                                       Case {3440.0f / 1440.0f, 102.88f}, Case {32.0f / 9.0f, 123.66f}})
	{
		EXPECT_FLOAT_EQ(VerticalFieldOfView(k_Lens, aspect), upAt4By3) << aspect;
		EXPECT_NEAR(glm::degrees(HorizontalFieldOfView(k_Lens, aspect)), acrossDegrees, 0.1f) << aspect;
		// The middle of the screen, a 4:3 part as tall as it, sees exactly the lens across
		const float middleAcross = std::tan(HorizontalFieldOfView(k_Lens, aspect) / 2.0f) * (4.0f / 3.0f) / aspect;
		EXPECT_NEAR(middleAcross, std::tan(k_Lens / 2.0f), 1e-5f) << aspect;
	}
}

// Any lens, such as a script's or the temple's, follows the same rule
TEST(CameraLens, EveryLensFollowsTheSameRule)
{
	for (const float degrees : {30.0f, 90.0f, 120.0f})
	{
		const float lens = glm::radians(degrees);
		EXPECT_FLOAT_EQ(HorizontalFieldOfView(lens, 4.0f / 3.0f), lens) << degrees;
		EXPECT_FLOAT_EQ(VerticalFieldOfView(lens, 16.0f / 9.0f), VerticalFieldOfView(lens, 4.0f / 3.0f)) << degrees;
		EXPECT_GT(HorizontalFieldOfView(lens, 16.0f / 9.0f), lens) << degrees;
	}
}
