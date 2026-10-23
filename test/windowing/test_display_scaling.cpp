/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <optional>
#include <string_view>

#include <SDL_hints.h>
#include <gtest/gtest.h>

#include "Windowing/DisplayScaling.h"

using namespace openblack::windowing;

namespace
{
std::optional<std::string_view> ValueOf(std::string_view name)
{
	const auto found = std::ranges::find(k_WindowsDisplayScalingHints, name, &SdlHint::name);
	if (found == k_WindowsDisplayScalingHints.end())
	{
		return std::nullopt;
	}
	return found->value;
}
} // namespace

TEST(DisplayScaling, WindowsDoesNotStretchTheWindowOnAnyMonitor)
{
	EXPECT_EQ(ValueOf(SDL_HINT_WINDOWS_DPI_AWARENESS), "permonitorv2");
}

TEST(DisplayScaling, OneUnitOfSdlIsOnePixelOfTheMonitor)
{
	// The window size, the back buffer and the pointer then use the same pixels at every scale
	EXPECT_EQ(ValueOf(SDL_HINT_WINDOWS_DPI_SCALING), "0");
}
