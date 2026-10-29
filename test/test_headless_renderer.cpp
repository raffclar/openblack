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

#include <bgfx/bgfx.h>
#include <gtest/gtest.h>

#include "Graphics/HeadlessRenderer.h"
#include "Graphics/ScreenshotCapture.h"

using openblack::graphics::HeadlessRenderer;

TEST(HeadlessRenderer, TheRendererMakesNoGraphicsObjectsAndDrawsFrames)
{
	auto renderer = HeadlessRenderer::Create();
	ASSERT_NE(renderer, nullptr);
	EXPECT_EQ(bgfx::getRendererType(), bgfx::RendererType::Noop);
	renderer->Frame();
	renderer->Frame();
	const auto* stats = bgfx::getStats();
	EXPECT_EQ(stats->numTextures, 0);
	EXPECT_EQ(stats->numVertexBuffers, 0);
	EXPECT_EQ(stats->numIndexBuffers, 0);
	EXPECT_EQ(stats->numDynamicVertexBuffers, 0);
	EXPECT_EQ(stats->numFrameBuffers, 0);
	EXPECT_EQ(stats->numShaders, 0);
	EXPECT_EQ(stats->numPrograms, 0);
	EXPECT_EQ(stats->numUniforms, 0);
	EXPECT_EQ(renderer->GetPipelinesMade(), 0u);
}

TEST(HeadlessRenderer, APictureNeverComesAndNeverFails)
{
	auto renderer = HeadlessRenderer::Create();
	ASSERT_NE(renderer, nullptr);
	const auto name = std::string(openblack::graphics::ScreenPictures::k_Prefix) + "challenge";
	bool picturerCalled = false;
	renderer->RequestScreenPicture(name, [&picturerCalled](const openblack::graphics::ScreenView& /*screen*/) {
		picturerCalled = true;
		return std::vector<uint8_t> {};
	});
	for (int frame = 0; frame < 3; ++frame)
	{
		renderer->Frame();
		EXPECT_FALSE(renderer->TakeScreenPicture(name).has_value());
		EXPECT_FALSE(renderer->TakeScreenshotFailure(name).has_value());
	}
	EXPECT_FALSE(picturerCalled);
}

TEST(HeadlessRenderer, TheDebugAndProfileFlagsKeepTheirValues)
{
	auto renderer = HeadlessRenderer::Create();
	ASSERT_NE(renderer, nullptr);
	EXPECT_FALSE(renderer->GetDebug());
	renderer->SetDebug(true);
	renderer->SetProfile(true);
	EXPECT_TRUE(renderer->GetDebug());
	EXPECT_TRUE(renderer->GetProfile());
}
