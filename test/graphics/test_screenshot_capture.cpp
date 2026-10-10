/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <filesystem>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/ScreenshotCapture.h"

using openblack::graphics::PlanScreenshot;
using openblack::graphics::ScreenshotFailures;
using openblack::graphics::ScreenshotRoute;
using openblack::graphics::UncapturedScreenshotPath;
using openblack::graphics::WriteScreenshotPng;

namespace
{
constexpr glm::u16vec2 k_WindowSize {1280, 1024};
} // namespace

// The screen is read as it is presented, unless the renderer keeps no screen while the window is minimised and it is
TEST(ScreenshotCapture, AMinimisedVulkanWindowIsDrawnAside)
{
	EXPECT_EQ(PlanScreenshot(true, false, true, k_WindowSize).route, ScreenshotRoute::Screen);
	EXPECT_EQ(PlanScreenshot(false, true, true, k_WindowSize).route, ScreenshotRoute::Screen);
	EXPECT_EQ(PlanScreenshot(false, true, false, k_WindowSize).route, ScreenshotRoute::Screen);
	EXPECT_EQ(PlanScreenshot(true, true, true, k_WindowSize).route, ScreenshotRoute::Offscreen);
}

// Drawn aside or not at all: without reading back, or without a size, the picture is refused with why
TEST(ScreenshotCapture, APictureThatCantBeDrawnAsideIsRefusedWithWhy)
{
	const auto noReadBack = PlanScreenshot(true, true, false, k_WindowSize);
	EXPECT_EQ(noReadBack.route, ScreenshotRoute::Refused);
	EXPECT_NE(noReadBack.why.find("restore the window"), std::string::npos);
	const auto noSize = PlanScreenshot(true, true, true, glm::u16vec2(0, 1024));
	EXPECT_EQ(noSize.route, ScreenshotRoute::Refused);
	EXPECT_NE(noSize.why.find("size"), std::string::npos);
}

// bgfx's note that it had no screen to read names the picture, its full stop aside; other notes name none
TEST(ScreenshotCapture, BgfxsNoteOfAPictureItCouldntTakeNamesIt)
{
	EXPECT_EQ(UncapturedScreenshotPath("Unable to capture screenshot E:/shots/a.b/frame_12.png."), "E:/shots/a.b/frame_12.png");
	EXPECT_EQ(UncapturedScreenshotPath("Unable to capture screenshot shot.png"), "shot.png");
	EXPECT_FALSE(UncapturedScreenshotPath("Unable to capture screenshot .").has_value());
	EXPECT_FALSE(UncapturedScreenshotPath("vkQueuePresentKHR(...): result = VK_ERROR_OUT_OF_DATE_KHR").has_value());
}

// A failure is told once, for its own path only
TEST(ScreenshotCapture, AFailureIsToldOnce)
{
	ScreenshotFailures failures;
	failures.Record("a.png", "no screen");
	EXPECT_FALSE(failures.Take("b.png").has_value());
	EXPECT_EQ(failures.Take("a.png"), "no screen");
	EXPECT_FALSE(failures.Take("a.png").has_value());
}

// The pixels are written whole under the picture's name, nothing left aside; too few pixels are refused
TEST(ScreenshotCapture, PixelsAreWrittenWhole)
{
	const auto folder = std::filesystem::temp_directory_path() / "openblack_test_screenshot_capture";
	std::error_code error;
	std::filesystem::remove_all(folder, error);
	std::filesystem::create_directories(folder);
	const auto path = folder / "shot.png";
	constexpr uint32_t k_Width = 4;
	constexpr uint32_t k_Height = 2;
	const std::vector<uint8_t> pixels(k_Width * k_Height * 4, 0x40);

	EXPECT_EQ(WriteScreenshotPng(path, k_Width, k_Height, k_Width * 4, pixels, false), "");
	EXPECT_TRUE(std::filesystem::exists(path));
	EXPECT_GT(std::filesystem::file_size(path), 0u);
	auto part = path;
	part += ".part";
	EXPECT_FALSE(std::filesystem::exists(part));

	const auto shortOf = folder / "short.png";
	EXPECT_NE(WriteScreenshotPng(shortOf, k_Width, k_Height + 1, k_Width * 4, pixels, false), "");
	EXPECT_FALSE(std::filesystem::exists(shortOf));
	std::filesystem::remove_all(folder, error);
}
