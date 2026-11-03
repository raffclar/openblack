/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>
#include <chrono>

#include <gtest/gtest.h>

#include "Graphics/WarmUpViews.h"

using namespace openblack::graphics;

namespace
{

[[nodiscard]] bool Kept(RenderPass pass)
{
	return std::ranges::find(warm_up::k_WindowSceneViews, pass) != warm_up::k_WindowSceneViews.end();
}

} // namespace

// Every view that the main scene draws into the window keeps to one pixel in a warm-up
TEST(WarmUpViews, MainSceneViewsKeepToOnePixel)
{
	EXPECT_TRUE(Kept(SkyPassOf(RenderPass::Main)));
	EXPECT_TRUE(Kept(LandPassOf(RenderPass::Main)));
	EXPECT_TRUE(Kept(RenderPass::Main));
	EXPECT_TRUE(Kept(TranslucentPassOf(RenderPass::Main)));
	EXPECT_TRUE(Kept(RenderPass::Advisors));
}

// The loading screen's view clears the window after every view of the scene, and draws over all of it
TEST(WarmUpViews, CoverComesAfterTheScene)
{
	for (const auto view : warm_up::k_WindowSceneViews)
	{
		EXPECT_LT(static_cast<int>(view), static_cast<int>(warm_up::k_CoverView));
	}
	EXPECT_EQ(warm_up::k_CoverView, RenderPass::Interface);
	EXPECT_FALSE(Kept(warm_up::k_CoverView));
}

// The views that draw into targets of their own are drawn whole: the next frames draw them again
TEST(WarmUpViews, TargetViewsAreDrawnWhole)
{
	for (const auto view : {RenderPass::LandShade, RenderPass::LandLuminosity, RenderPass::LandColour, RenderPass::SkyDome,
	                        RenderPass::ObjectShadow, RenderPass::Reflection, RenderPass::ReflectionSky,
	                        RenderPass::ReflectionTranslucent, RenderPass::ReflectionMirrored, RenderPass::UnderSea})
	{
		EXPECT_FALSE(Kept(view));
	}
}

// The loading screen waits for some frames after a prefetch, then while pipelines are not ready
TEST(WarmUpViews, PrefetchWaitsForSettleFramesThenPipelines)
{
	using std::chrono::milliseconds;
	for (uint32_t frames = 0; frames < warm_up::k_PrefetchSettleFrames; ++frames)
	{
		EXPECT_TRUE(warm_up::PrefetchWaits(frames, 0, milliseconds {0})) << frames;
	}
	EXPECT_FALSE(warm_up::PrefetchWaits(warm_up::k_PrefetchSettleFrames, 0, milliseconds {0}));
	EXPECT_TRUE(warm_up::PrefetchWaits(warm_up::k_PrefetchSettleFrames, 3, milliseconds {100}));
	EXPECT_TRUE(warm_up::PrefetchWaits(50, 1, warm_up::k_PrefetchWaitLimit - milliseconds {1}));
}

// After the limit, the loading screen stops the wait. The warm-up then makes the pipelines that are not ready.
TEST(WarmUpViews, PrefetchStopsAtTheLimit)
{
	EXPECT_FALSE(warm_up::PrefetchWaits(0, 0, warm_up::k_PrefetchWaitLimit));
	EXPECT_FALSE(warm_up::PrefetchWaits(10, 25, warm_up::k_PrefetchWaitLimit));
}
