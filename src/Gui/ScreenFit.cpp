/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScreenFit.h"

using namespace openblack::gui;

ScreenRect openblack::gui::FitInside(Aspect aspect, int32_t screenWidth, int32_t screenHeight) noexcept
{
	if (aspect.width <= 0 || aspect.height <= 0 || screenWidth <= 0 || screenHeight <= 0)
	{
		return {.x = 0, .y = 0, .width = screenWidth, .height = screenHeight};
	}
	// Compared and measured in whole numbers: the rows (or columns) the picture leaves over, rounded down
	const auto width = static_cast<int64_t>(screenWidth);
	const auto height = static_cast<int64_t>(screenHeight);
	const int64_t tallness = height * aspect.width;
	const int64_t wideness = width * aspect.height;
	if (tallness >= wideness)
	{
		// As wide as the screen, bars above and below
		const auto bar = static_cast<int32_t>((tallness - wideness) / aspect.width / 2);
		return {.x = 0, .y = bar, .width = screenWidth, .height = screenHeight - (2 * bar)};
	}
	// As tall as the screen, bars at the sides
	const auto bar = static_cast<int32_t>((wideness - tallness) / aspect.height / 2);
	return {.x = bar, .y = 0, .width = screenWidth - (2 * bar), .height = screenHeight};
}
