/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/GraphicsLibrary.h"
#include "Graphics/HeadlessRenderer.h"
#include "Graphics/ScreenshotCapture.h"

using openblack::graphics::HeadlessRenderer;

TEST(HeadlessRenderer, TheRendererDoesNotStartTheGraphicsLibraryAndDrawsFrames)
{
	ASSERT_FALSE(openblack::graphics::GraphicsLibraryStarted());
	HeadlessRenderer renderer;
	renderer.ConfigureView(openblack::graphics::RenderPass::Main, {640, 480}, 0);
	renderer.Reset({640, 480});
	renderer.BakeLandShadows(true);
	renderer.PrepareCreatureSkins(4);
	renderer.ReleaseGpuWorkMemory();
	renderer.Frame();
	renderer.Frame();
	EXPECT_FALSE(openblack::graphics::GraphicsLibraryStarted());
	EXPECT_EQ(renderer.GetPipelinesMade(), 0u);
	EXPECT_EQ(renderer.GetDrawsOfLastFrame(), 0u);
}

TEST(HeadlessRenderer, APictureNeverComesAndNeverFails)
{
	HeadlessRenderer renderer;
	const auto name = std::string(openblack::graphics::ScreenPictures::k_Prefix) + "challenge";
	bool picturerCalled = false;
	renderer.RequestScreenPicture(name, [&picturerCalled](const openblack::graphics::ScreenView& /*screen*/) {
		picturerCalled = true;
		return std::vector<uint8_t> {};
	});
	for (int frame = 0; frame < 3; ++frame)
	{
		renderer.Frame();
		EXPECT_FALSE(renderer.TakeScreenPicture(name).has_value());
		EXPECT_FALSE(renderer.TakeScreenshotFailure(name).has_value());
	}
	EXPECT_FALSE(picturerCalled);
}

TEST(HeadlessRenderer, TheDebugAndProfileFlagsKeepTheirValues)
{
	HeadlessRenderer renderer;
	EXPECT_FALSE(renderer.GetDebug());
	renderer.SetDebug(true);
	renderer.SetProfile(true);
	EXPECT_TRUE(renderer.GetDebug());
	EXPECT_TRUE(renderer.GetProfile());
}
