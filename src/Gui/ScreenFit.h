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

/// Where a full-screen picture goes on a screen of any shape: as big as fits, keeping its own shape, centred, the rest
/// of the screen left to black bars. The game was made for 4:3 screens, where its 4:3 pictures fill the screen and its
/// 16:9 films sit between bars; on other shapes they keep their shape the same way.
namespace openblack::gui
{

/// A picture's shape, as its width to its height
struct Aspect
{
	int32_t width {4};
	int32_t height {3};

	bool operator==(const Aspect&) const = default;
};

/// The shape the game's own screens (logos, tips, pre-intro) are shown at
inline constexpr Aspect k_FourByThree {.width = 4, .height = 3};
/// The shape of the game's films
inline constexpr Aspect k_SixteenByNine {.width = 16, .height = 9};

/// A rectangle on the screen in pixels, from its top left corner
struct ScreenRect
{
	int32_t x {0};
	int32_t y {0};
	int32_t width {0};
	int32_t height {0};

	bool operator==(const ScreenRect&) const = default;
};

/// The biggest rectangle of `aspect` inside a screen, centred. The pixels it leaves over are shared between two bars,
/// each half of them rounded towards zero, as the game shares what a 16:9 film leaves of a 4:3 screen. A screen of
/// the picture's own shape is filled exactly
[[nodiscard]] ScreenRect FitInside(Aspect aspect, int32_t screenWidth, int32_t screenHeight) noexcept;

} // namespace openblack::gui
