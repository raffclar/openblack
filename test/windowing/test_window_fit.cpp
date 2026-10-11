/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Windowing/WindowFit.h"

using namespace openblack::windowing;

namespace
{
// A 5120x1440 screen with a 40 pixel taskbar along the bottom, and a usual window frame with its title bar
constexpr DesktopRect k_Usable {.position = {0, 0}, .size = {5120, 1400}};
constexpr WindowBorders k_Frame {.top = 31, .left = 8, .bottom = 8, .right = 8};
} // namespace

TEST(WindowFit, AWindowAlreadyOnTheDesktopStaysWhereItIs)
{
	EXPECT_FALSE(FitWindow({.position = {100, 100}, .size = {1280, 1024}}, k_Frame, k_Usable).has_value());
}

TEST(WindowFit, AWindowHangingOffTheDesktopIsCentredOnIt)
{
	const auto fitted = FitWindow({.position = {4500, 600}, .size = {1280, 1024}}, k_Frame, k_Usable);
	ASSERT_TRUE(fitted.has_value());
	EXPECT_EQ(fitted->size, glm::ivec2(1280, 1024));
	EXPECT_EQ(fitted->position, glm::ivec2((5120 - 1296) / 2 + 8, (1400 - 1063) / 2 + 31));
}

TEST(WindowFit, AWindowAsTallAsTheScreenIsMadeSmallerKeepingItsShape)
{
	// 2560x1440 on a screen 1440 tall: the bottom of the picture went under the taskbar
	const auto fitted = FitWindow({.position = {1280, 0}, .size = {2560, 1440}}, k_Frame, k_Usable);
	ASSERT_TRUE(fitted.has_value());
	EXPECT_EQ(fitted->size.y, 1400 - 31 - 8);
	EXPECT_NEAR(static_cast<float>(fitted->size.x) / static_cast<float>(fitted->size.y), 16.0f / 9.0f, 0.002f);
	// The whole window, frame and all, is inside the usable part of the desktop
	EXPECT_GE(fitted->position.y - k_Frame.top, 0);
	EXPECT_LE(fitted->position.y + fitted->size.y + k_Frame.bottom, 1400);
	EXPECT_GE(fitted->position.x - k_Frame.left, 0);
	EXPECT_LE(fitted->position.x + fitted->size.x + k_Frame.right, 5120);
}

TEST(WindowFit, AWindowTooWideIsMadeNarrower)
{
	constexpr DesktopRect usable {.position = {0, 0}, .size = {1920, 1040}};
	const auto fitted = FitWindow({.position = {0, 31}, .size = {3440, 1440}}, k_Frame, usable);
	ASSERT_TRUE(fitted.has_value());
	EXPECT_EQ(fitted->size.x, 1920 - 16);
	EXPECT_LE(fitted->size.y, 1040 - 39);
}

TEST(WindowFit, ADesktopNotStartingAtZeroIsRespected)
{
	// A second screen to the left of the main one, with its taskbar along the top
	constexpr DesktopRect usable {.position = {-1920, 40}, .size = {1920, 1040}};
	const auto fitted = FitWindow({.position = {-1920, 0}, .size = {1280, 720}}, k_Frame, usable);
	ASSERT_TRUE(fitted.has_value());
	EXPECT_GE(fitted->position.y - k_Frame.top, 40);
	EXPECT_GE(fitted->position.x - k_Frame.left, -1920);
	EXPECT_LE(fitted->position.x + fitted->size.x + k_Frame.right, 0);
}
