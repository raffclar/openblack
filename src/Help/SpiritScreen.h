/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <algorithm>

/// The screen the advisors move on. The game was made for 4:3 screens and measures the advisors' hover space in half
/// the screen's width. On a wider screen the advisors use the whole screen, but they are measured in half the width of
/// a 4:3 screen of the same height (the camera keeps a 4:3 screen's view up and down there), so their sizes, speeds and
/// everything up and down stay as on a 4:3 screen while their places reach out to the real screen's sides. Screens 4:3
/// or taller are measured exactly as the game measures them.
namespace openblack::help::spirits
{

/// A screen size in pixels
struct Screen
{
	uint16_t width {640};
	uint16_t height {480};
	[[nodiscard]] int32_t HalfWidth() const { return width >> 1; }
	[[nodiscard]] int32_t HalfHeight() const { return height >> 1; }

	/// One unit of the advisors' hover space in pixels: half the screen's width, or on a screen wider than 4:3 half the
	/// width of a 4:3 screen of its height
	[[nodiscard]] int32_t HoverUnit() const
	{
		return std::max(1, std::min(HalfWidth(), (static_cast<int32_t>(height) * 2) / 3));
	}
	/// The left edge of the screen's 4:3 middle in pixels, where the hover space's -1 lies: 0 on screens 4:3 or taller
	[[nodiscard]] int32_t MiddleLeft() const { return HalfWidth() - HoverUnit(); }
	/// How far the side edges lie from the middle of the screen, in hover units: 1 on screens 4:3 or taller
	[[nodiscard]] float Reach() const { return static_cast<float>(HalfWidth()) / static_cast<float>(HoverUnit()); }

	bool operator==(const Screen&) const = default;
};

} // namespace openblack::help::spirits
