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
	                        RenderPass::Footprint, RenderPass::LandAlpha, RenderPass::ObjectShadow, RenderPass::Reflection,
	                        RenderPass::ReflectionSky, RenderPass::ReflectionTranslucent, RenderPass::UnderSea})
	{
		EXPECT_FALSE(Kept(view));
	}
}
