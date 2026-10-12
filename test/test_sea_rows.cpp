/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <chrono>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "Graphics/SeaRows.h"

namespace sea_rows = openblack::graphics::sea_rows;

namespace
{
constexpr glm::vec2 k_Viewport {1280.0f, 720.0f};
constexpr float k_Near = 1.0f;

glm::mat4 ViewProjection(const glm::vec3& eye, const glm::vec3& focus)
{
	const auto projection = glm::perspective(glm::radians(60.0f), k_Viewport.x / k_Viewport.y, k_Near, 100000.0f);
	return projection * glm::lookAt(eye, focus, {0.0f, 1.0f, 0.0f});
}
} // namespace

TEST(SeaRows, PeriodFollowsTheWaterTiling)
{
	EXPECT_FLOAT_EQ(sea_rows::Period(0.0f), 2000.0f);
	EXPECT_FLOAT_EQ(sea_rows::Period(0.8f), 560.0f);
	EXPECT_FLOAT_EQ(sea_rows::Period(1.0f), 200.0f);
}

TEST(SeaRows, LookingDownTheSeaFillsTheScreen)
{
	const auto range =
	    sea_rows::ComputeScreenRange(ViewProjection({2560.0f, 1000.0f, 2560.0f}, {2560.0f, 0.0f, 2561.0f}), k_Viewport, k_Near);
	ASSERT_TRUE(range.has_value());
	EXPECT_FLOAT_EQ(range->top, 0.0f);
	EXPECT_FLOAT_EQ(range->bottom, k_Viewport.y - 1.0f);
}

TEST(SeaRows, LookingAcrossTheSeaStartsNearTheHorizon)
{
	const auto range = sea_rows::ComputeScreenRange(ViewProjection({2560.0f, 100.0f, 2560.0f}, {2560.0f, 100.0f, 3560.0f}),
	                                                k_Viewport, k_Near);
	ASSERT_TRUE(range.has_value());
	// The square's far edge, 15000 units away, is a few pixels under the horizon in the middle of the screen
	EXPECT_GT(range->top, k_Viewport.y / 2.0f);
	EXPECT_LT(range->top, k_Viewport.y / 2.0f + 10.0f);
	EXPECT_FLOAT_EQ(range->bottom, k_Viewport.y - 1.0f);
	// Further away at the top than at the bottom
	EXPECT_LT(range->inverseDepthTop, range->inverseDepthBottom);
}

TEST(SeaRows, NoRowsWhenTheSeaIsBehind)
{
	// Beyond the square's edge, looking away from it
	EXPECT_FALSE(sea_rows::ComputeScreenRange(ViewProjection({2560.0f, 100.0f, 40000.0f}, {2560.0f, 100.0f, 41000.0f}),
	                                          k_Viewport, k_Near)
	                 .has_value());
	// Looking up at the sky
	EXPECT_FALSE(sea_rows::ComputeScreenRange(ViewProjection({2560.0f, 100.0f, 2560.0f}, {2560.0f, 2000.0f, 2561.0f}),
	                                          k_Viewport, k_Near)
	                 .has_value());
}

namespace
{
/// The game's own view: 70 degrees across the width and no far plane, looking from a height along a yaw and a pitch
/// below the horizon
glm::mat4 HighViewProjection(glm::vec2 viewport, float height, float yawDegrees, float pitchDegrees)
{
	const float yFov = 2.0f * std::atan(std::tan(glm::radians(70.0f) / 2.0f) * viewport.y / viewport.x);
	const auto projection = glm::infinitePerspective(yFov, viewport.x / viewport.y, 3.5f);
	const glm::vec3 eye {2560.0f, height, 2560.0f};
	const float yaw = glm::radians(yawDegrees);
	const float pitch = glm::radians(pitchDegrees);
	const glm::vec3 forward {std::cos(pitch) * std::sin(yaw), -std::sin(pitch), std::cos(pitch) * std::cos(yaw)};
	return projection * glm::lookAt(eye, eye + forward, {0.0f, 1.0f, 0.0f});
}

/// One over the view depth of the sea under screen row y, straight from the view
float InverseDepthUnderRow(const glm::mat4& viewProjection, glm::vec2 viewport, float y)
{
	const auto inverse = glm::inverse(viewProjection);
	const float ndcY = 1.0f - ((y / viewport.y) * 2.0f);
	auto near = inverse * glm::vec4(0.0f, ndcY, -1.0f, 1.0f);
	auto far = inverse * glm::vec4(0.0f, ndcY, 0.0f, 1.0f);
	near /= near.w;
	far /= far.w;
	const glm::vec3 direction = glm::vec3(far - near);
	const glm::vec3 point = glm::vec3(near) + direction * (-near.y / direction.y);
	return 1.0f / (viewProjection * glm::vec4(point, 1.0f)).w;
}
} // namespace

// From as high as the camera goes (30000) the square of sea still gives whole rows down to the bottom of the screen,
// their depths running smoothly from the far edge to the bottom
TEST(SeaRows, TheRowsHoldFromAsHighAsTheCameraGoes)
{
	for (const glm::vec2 viewport : {glm::vec2 {1920.0f, 1080.0f}, glm::vec2 {1280.0f, 1024.0f}, glm::vec2 {5120.0f, 1440.0f}})
	{
		for (const float height : {5000.0f, 15000.0f, 30000.0f})
		{
			for (const float pitch : {50.0f, 70.0f, 89.0f})
			{
				const auto viewProjection = HighViewProjection(viewport, height, 30.0f, pitch);
				const auto range = sea_rows::ComputeScreenRange(viewProjection, viewport, 3.5f);
				ASSERT_TRUE(range.has_value()) << height << " " << pitch;
				EXPECT_GE(range->top, 0.0f);
				EXPECT_LE(range->bottom, viewport.y - 1.0f);
				EXPECT_LT(range->top, range->bottom);
				const auto rows = sea_rows::MakeRows(*range);
				EXPECT_GT(rows.count, 0);
				EXPECT_LE(rows.first + (2 * rows.count), static_cast<int>(viewport.y) + 2);
				// Far but finite, and nearer down the screen
				EXPECT_GT(rows.inverseDepth, 0.0f);
				EXPECT_TRUE(std::isfinite(rows.inverseStep));
				EXPECT_GE(rows.inverseStep, 0.0f);
				const float last = rows.inverseDepth + (static_cast<float>(rows.count - 1) * rows.inverseStep);
				EXPECT_NEAR(last, range->inverseDepthBottom, range->inverseDepthBottom * 1e-4f);
				// The depths are those of the sea under the rows: no more than 30000 units of height away from it
				EXPECT_LE(1.0f / range->inverseDepthBottom, height * 1.0001f / std::sin(glm::radians(pitch - 25.0f)));
				if (range->bottom >= viewport.y - 1.0f && range->top > 0.0f)
				{
					EXPECT_NEAR(range->inverseDepthBottom, InverseDepthUnderRow(viewProjection, viewport, viewport.y - 1.0f),
					            range->inverseDepthBottom * 2e-3f)
					    << height << " " << pitch;
				}
			}
		}
	}
}

// Looking out from 30000, the square is far below: it is seen only in the lower part of the screen, its far corner
// below the middle, and the first row fades in. Nearly level, it is below the screen altogether
TEST(SeaRows, FromHighLookingOutTheSeaLiesLow)
{
	const glm::vec2 viewport {1920.0f, 1080.0f};
	EXPECT_FALSE(
	    sea_rows::ComputeScreenRange(HighViewProjection(viewport, 30000.0f, 30.0f, 15.0f), viewport, 3.5f).has_value());
	const auto range = sea_rows::ComputeScreenRange(HighViewProjection(viewport, 30000.0f, 30.0f, 45.0f), viewport, 3.5f);
	ASSERT_TRUE(range.has_value());
	EXPECT_GT(range->top, viewport.y / 2.0f);
	EXPECT_FLOAT_EQ(range->bottom, viewport.y - 1.0f);
	EXPECT_TRUE(sea_rows::MakeRows(*range).softTop);
}

TEST(SeaRows, RowsAreTwoPixelsApart)
{
	const auto rows =
	    sea_rows::MakeRows({.top = 100.7f, .bottom = 719.0f, .inverseDepthTop = 0.001f, .inverseDepthBottom = 0.1f});
	EXPECT_EQ(rows.first, 100);
	EXPECT_EQ(rows.count, 310);
	EXPECT_TRUE(rows.softTop);
	EXPECT_FLOAT_EQ(rows.inverseDepth, 0.001f);
	EXPECT_FLOAT_EQ(rows.inverseDepth + static_cast<float>(rows.count - 1) * rows.inverseStep, 0.1f);

	// From the top of the screen the first row isn't faded
	EXPECT_FALSE(
	    sea_rows::MakeRows({.top = 0.0f, .bottom = 719.0f, .inverseDepthTop = 0.001f, .inverseDepthBottom = 0.1f}).softTop);
}

// The ripple's pattern is the game's on its default screen, 800 wide, and runs over the same share of the view on any
// other: as the camera keeps the game's horizontal field of view, a row of a wider screen spans less of it
TEST(SeaRows, TheRipplesPatternCoversTheShareOfTheViewItDoesOnTheDefaultScreen)
{
	EXPECT_FLOAT_EQ(sea_rows::RippleRowsPerRow(800.0f), 1.0f);
	for (const float width : {640.0f, 1280.0f, 1920.0f, 2560.0f, 3440.0f, 5120.0f})
	{
		// The whole width of rows down the screen spans as many of the pattern's rows as 800 rows of the default screen
		EXPECT_FLOAT_EQ(width * sea_rows::RippleRowsPerRow(width), 800.0f) << width;
	}
}

TEST(SeaRows, TheRippleStepsOnAtTheScreenRefreshWhateverTheFrameRate)
{
	using std::chrono::milliseconds;
	// A second of frames (and a little more) at 500 frames a second steps 60 times, as at a 60 Hz refresh
	sea_rows::Ripple fast;
	int fastSteps = 0;
	for (int frame = 0; frame < 505; ++frame)
	{
		const auto before = fast.step;
		fast = sea_rows::AdvanceRipple(fast, milliseconds(2), true);
		fastSteps += fast.step != before ? 1 : 0;
	}
	EXPECT_EQ(fastSteps, 60);

	// At 100 frames a second also 60 steps
	sea_rows::Ripple hundred;
	int hundredSteps = 0;
	for (int frame = 0; frame < 101; ++frame)
	{
		const auto before = hundred.step;
		hundred = sea_rows::AdvanceRipple(hundred, milliseconds(10), true);
		hundredSteps += hundred.step != before ? 1 : 0;
	}
	EXPECT_EQ(hundredSteps, 60);
}

TEST(SeaRows, SlowFramesStepTheRippleOnceEach)
{
	// At 20 frames a second the game stepped once a frame, never twice
	sea_rows::Ripple ripple;
	for (int frame = 1; frame <= 20; ++frame)
	{
		ripple = sea_rows::AdvanceRipple(ripple, std::chrono::milliseconds(50), true);
		EXPECT_EQ(ripple.step, frame % sea_rows::Ripple::k_Steps);
		EXPECT_LT(ripple.sinceStep, sea_rows::Ripple::k_StepInterval);
	}
}

TEST(SeaRows, TheRippleStaysWhileTheGamesTimeIsStopped)
{
	sea_rows::Ripple ripple {.step = 15};
	ripple = sea_rows::AdvanceRipple(ripple, std::chrono::milliseconds(100), false);
	EXPECT_EQ(ripple.step, 15);
	// And goes round from 15 to 0
	ripple = sea_rows::AdvanceRipple(ripple, std::chrono::milliseconds(100), true);
	EXPECT_EQ(ripple.step, 0);
}
